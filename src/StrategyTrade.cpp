#include <vector>
#include <string>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <memory>
#include "StrategyTrade.h"
#include "CustomTradeSpi.h"

extern std::unordered_map<std::string, TickToKlineHelper> g_KlineHash;


// 线程互斥量
std::mutex marketDataMutex;

void StrategyCheckAndTrade(TThostFtdcInstrumentIDType instrumentID, CustomTradeSpi *customTradeSpi)
{
	// 加锁
	std::lock_guard<std::mutex> lk(marketDataMutex);
	TickToKlineHelper& tickToKlineObject = g_KlineHash.at(std::string(instrumentID));
	// 策略
	std::vector<double> priceVec = tickToKlineObject.m_priceVec;
	if (priceVec.size() >= 3)
	{
		int len = priceVec.size();
		// 最后连续三个上涨就买开仓,反之就卖开仓,这里暂时用最后一个价格下单
		if (priceVec[len - 1] > priceVec[len - 2] && priceVec[len - 2] > priceVec[len - 3])
			customTradeSpi->reqOrderInsert(instrumentID, priceVec[len - 1], 1, THOST_FTDC_D_Buy);
		else if (priceVec[len - 1] < priceVec[len - 2] && priceVec[len - 2] < priceVec[len - 3])
			customTradeSpi->reqOrderInsert(instrumentID, priceVec[len - 1], 1, THOST_FTDC_D_Sell);
	}
}

void PivotReversalStrategy::operator()()
{
	if (status >= 8) {
		return;
	}
	else {
		if (taskQue.size() > 0) {
			taskQue.front();
			taskQue.pop();
		}
	}
	std::lock_guard<std::mutex> lk(strategyMutex);
	double swh = pivot(Strategy::Type::high);
	double swl = pivot(Strategy::Type::low);
	if (swh <= 0.0 ) {
		if (highPivotQue.empty()) {
			return;
		}
		swh = highPivotQue.back();
	}

	if (swl <= 0.0) {
		if (lowPivotQue.empty()) {
			return;
		}
		swl = lowPivotQue.back();
	}
	TickToKlineHelper& tickToKlineObject = g_KlineHash.at(instrumentID);
	if (tickToKlineObject.lastPrice > swh) {
		if (status == 0) {
			preStatus = 0;
			status = status | 8;
			std::shared_ptr<CThostFtdcInputOrderField> orderInsertReq = std::make_shared<CThostFtdcInputOrderField>();
			memset(orderInsertReq.get(), 0, sizeof(CThostFtdcInputOrderField));
			strcpy(orderInsertReq->InstrumentID, instrumentID.c_str());
			orderInsertReq->Direction = THOST_FTDC_D_Buy;
			orderInsertReq->CombOffsetFlag[0] = THOST_FTDC_OF_Open;
			orderInsertReq->LimitPrice = tickToKlineObject.lastPrice * volume;
			orderInsertReq->VolumeTotalOriginal = volume;
			customTradeSpi->reqOrder(orderInsertReq);
			
			status = 8 | 1;
		}
		else if (status == 2) {
			preStatus = 2;
			{
				status = status | 8;
				std::shared_ptr<CThostFtdcInputOrderField> orderInsertReq = std::make_shared<CThostFtdcInputOrderField>();
				memset(orderInsertReq.get(), 0, sizeof(CThostFtdcInputOrderField));
				strcpy(orderInsertReq->InstrumentID, instrumentID.c_str());
				orderInsertReq->Direction = THOST_FTDC_D_Sell;
				orderInsertReq->CombOffsetFlag[0] = THOST_FTDC_OF_Close;
				orderInsertReq->LimitPrice = tickToKlineObject.lastPrice * volume;
				orderInsertReq->VolumeTotalOriginal = volume;
				customTradeSpi->reqOrder(orderInsertReq);
			}
			auto lastPrice = tickToKlineObject.lastPrice;
			taskQue.push([this, swh](){
				preStatus = 0;
				status = preStatus | 8;
				std::lock_guard<std::mutex> lk(strategyMutex);
				TickToKlineHelper& tickToKlineObject = g_KlineHash.at(std::string(instrumentID));
				if (tickToKlineObject.lastPrice > swh) {
					std::shared_ptr<CThostFtdcInputOrderField> orderInsertReq = std::make_shared<CThostFtdcInputOrderField>();
					memset(orderInsertReq.get(), 0, sizeof(CThostFtdcInputOrderField));
					strcpy(orderInsertReq->InstrumentID, instrumentID.c_str());
					orderInsertReq->Direction = THOST_FTDC_D_Buy;
					orderInsertReq->CombOffsetFlag[0] = THOST_FTDC_OF_Open;
					orderInsertReq->LimitPrice = tickToKlineObject.lastPrice * volume;
					orderInsertReq->VolumeTotalOriginal = volume;
					customTradeSpi->reqOrder(orderInsertReq);
					status = 8 | 1;
				}
				else {
					status = 0;
				}
				
			});
			
		}
	}
	if (tickToKlineObject.lastPrice < swl) {
		if (status == 0) {
			preStatus = 0;
			status = status | 8;
			std::shared_ptr<CThostFtdcInputOrderField> orderInsertReq = std::make_shared<CThostFtdcInputOrderField>();
			memset(orderInsertReq.get(), 0, sizeof(CThostFtdcInputOrderField));
			strcpy(orderInsertReq->InstrumentID, instrumentID.c_str());
			orderInsertReq->Direction = THOST_FTDC_D_Sell;
			orderInsertReq->CombOffsetFlag[0] = THOST_FTDC_OF_Open;
			orderInsertReq->LimitPrice = tickToKlineObject.lastPrice * volume;
			orderInsertReq->VolumeTotalOriginal = volume;
			customTradeSpi->reqOrder(orderInsertReq);
			status = 8 | 2;
		}
		else if (status == 1) {
			preStatus = 1;
			status = status | 8;
			{
				std::shared_ptr<CThostFtdcInputOrderField> orderInsertReq = std::make_shared<CThostFtdcInputOrderField>();
				memset(orderInsertReq.get(), 0, sizeof(CThostFtdcInputOrderField));
				strcpy(orderInsertReq->InstrumentID, instrumentID.c_str());
				orderInsertReq->Direction = THOST_FTDC_D_Buy;
				orderInsertReq->CombOffsetFlag[0] = THOST_FTDC_OF_Close;
				orderInsertReq->LimitPrice = tickToKlineObject.lastPrice * volume;
				orderInsertReq->VolumeTotalOriginal = volume;
				customTradeSpi->reqOrder(orderInsertReq);
			}
			auto lastPrice = tickToKlineObject.lastPrice;
			taskQue.push([this, swl]() {
				preStatus = 0;
				status = preStatus | 8;
				std::lock_guard<std::mutex> lk(strategyMutex);
				TickToKlineHelper& tickToKlineObject = g_KlineHash.at(std::string(instrumentID));
				if (tickToKlineObject.lastPrice < swl) {
					std::shared_ptr<CThostFtdcInputOrderField> orderInsertReq = std::make_shared<CThostFtdcInputOrderField>();
					memset(orderInsertReq.get(), 0, sizeof(CThostFtdcInputOrderField));
					strcpy(orderInsertReq->InstrumentID, instrumentID.c_str());
					orderInsertReq->Direction = THOST_FTDC_D_Sell;
					orderInsertReq->CombOffsetFlag[0] = THOST_FTDC_OF_Open;
					orderInsertReq->LimitPrice = tickToKlineObject.lastPrice * volume;
					orderInsertReq->VolumeTotalOriginal = volume;
					customTradeSpi->reqOrder(orderInsertReq);
					status = 8 | 2;
				}
				else {
					status = 0;
				}
			});

		}
	}
}

double PivotReversalStrategy::pivot(Strategy::Type type) {
	int range = left + right;
	std::vector<double> pivotArray;
	TickToKlineHelper& tickToKlineObject = g_KlineHash.at(instrumentID);
	if (tickToKlineObject.m_KLineDataArray.size() < range) {
		return 0.0;
	}

	size_t size = tickToKlineObject.m_KLineDataArray.size();
	bool isMin = false;
	switch (type) {
	case Strategy::Type::high:
	{
		if (size == barsNumHigh) {
			return 0.0;
		}
		else {
			barsNumHigh = size;
		}
		int i = range;
		for (auto it = tickToKlineObject.m_KLineDataArray.rbegin(); i > 0; i--, it++) {
			pivotArray.push_back(it->high_price);
		}
		break;
	}
	case Strategy::Type::low:
	{
		if (size == barsNumLow) {
			return 0.0;
		}
		else {
			barsNumLow = size;
		}
		int i = range;
		for (auto it = tickToKlineObject.m_KLineDataArray.rbegin(); i > 0; i--, it++) {
			pivotArray.push_back(it->low_price);
		}
		isMin = true;
		break;
	}
	case Strategy::Type::open:
		break;
	case Strategy::Type::close:
		break;
	}
	double pivotVal = pivotArray[right];
	for (int i = 0; i < range; i++) {
		if (i < right) {
			if (isMin) {
				if (pivotVal >= pivotArray[i] || fabs(pivotVal - pivotArray[i]) < 0.0005) {
					return 0.0;
				}
			}
			else {
				if (pivotVal <= pivotArray[i]|| fabs(pivotVal - pivotArray[i]) < 0.0005) {
					return 0.0;
				}
			}
		}
		else if (i > right) {
			if (isMin) {
				if (pivotVal > pivotArray[i]) {
					return 0.0;
				}
			}
			else {
				if (pivotVal < pivotArray[i]) {
					return 0.0;
				}
			}
		}
	}
	if (!isMin) {
		highPivotQue.push_back(pivotVal);
		outFile << "high:" << pivotVal<< std::endl;
	}
	else {
		lowPivotQue.push_back(pivotVal);
		outFile << "low:" << pivotVal << std::endl;
	}
	
	return pivotVal;
}
