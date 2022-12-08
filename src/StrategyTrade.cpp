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
	if (!opStart) {
		opStart = true;
		curVolume = 0;
	}
	if (status >= 8) {
		return;
	}
	else {
		if (taskQue.size() > 0) {
			taskQue.front()();
			taskQue.pop_front();
		}
	}
	std::lock_guard<std::mutex> lk(strategyMutex);
	double swh = pivot(Strategy::Type::high);
	double swl = pivot(Strategy::Type::low);
	if (swh <= 0.0 ) {
		if (highPivotQue.empty()) {
			return;
		}
		swh = highPivotQue.back().first;
	}

	if (swl <= 0.0) {
		if (lowPivotQue.empty()) {
			return;
		}
		swl = lowPivotQue.back().first;
	}
	TickToKlineHelper& tickToKlineObject = g_KlineHash.at(instrumentID);
	if (tickToKlineObject.lastPrice > swh) {
		int size = tickToKlineObject.m_KLineDataArray.size();
		double v = tickToKlineObject.lastPrice - tickToKlineObject.m_KLineDataArray[size - right].high_price;
		if (status == 0) {
			this->preStatus = 0;
			
			if (v > -(highPivotQue.back().second)) {
				makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Open, volume);
				this->status = 8 | 1;
			}
		}
		else if (status == 2) {
			preStatus = 2;
			{
				makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, curVolume.load());
				//this->status = 8;
			}
			auto lastPrice = tickToKlineObject.lastPrice;
			double hv = highPivotQue.back().second;
			taskQue.emplace_back([this, swh, v, hv](){
				this->preStatus = 0;
				std::lock_guard<std::mutex> lk(strategyMutex);
				TickToKlineHelper& tickToKlineObject = g_KlineHash.at(this->instrumentID);
				if (tickToKlineObject.lastPrice > swh && v > -hv) {
					makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Open, volume);
					this->status = 8 | 1;
				}
				else {
					this->status = 0;
				}
				
			});
			
		}
		else if (status == 1 && curVolume < volume) {
			this->preStatus = 1;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Open, volume - curVolume);
			this->status = 8 | 1;
		}
	}
	if (tickToKlineObject.lastPrice < swl) {
		int size = tickToKlineObject.m_KLineDataArray.size();
		double v = tickToKlineObject.lastPrice - tickToKlineObject.m_KLineDataArray[size - right].low_price;
		if (status == 0) {
			preStatus = 0;
			if (-v > lowPivotQue.back().second) {
				makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_Open, volume);
				status = 8 | 2;
			}
		}
		else if (status == 1) {
			this->preStatus = 1;
			{
				makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, curVolume.load());
				//this->status = 8;
			}
			auto lastPrice = tickToKlineObject.lastPrice;
			double lV = lowPivotQue.back().second;
			taskQue.emplace_back([this, swl, v, lV]() {
				std::lock_guard<std::mutex> lk(strategyMutex);
				this->preStatus = 0;
				TickToKlineHelper& tickToKlineObject = g_KlineHash.at(this->instrumentID);
				if (tickToKlineObject.lastPrice < swl && -v > lV) {
					makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_Open, volume);
					this->status = 8 | 2;
				}
				else {
					this->status = 0;
				}
			});

		}
		else if (status == 2 && curVolume < volume) {
			this->preStatus = 2;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Open, volume - curVolume);
			this->status = 8 | 2;
		}
	}
}

void PivotReversalStrategy::clearInvestor(CThostFtdcInvestorPositionField investor, int status, bool isLast) {
	if (status <= 0) {
		return;
	}
	tasks.emplace_back([this, investor, status, isLast]() {

		int limit = instrumentField.MaxMarketOrderVolume / 3;
		limit = limit > instrumentField.MinMarketOrderVolume ? limit : instrumentField.MinMarketOrderVolume;
		std::lock_guard<std::mutex> lk(strategyMutex);

		TickToKlineHelper& tickToKlineObject = g_KlineHash.at(instrumentID);
		if (status & 1 != 0) {
			int YdPosition = investor.Position - investor.TodayPosition;
			if (investor.PosiDirection == THOST_FTDC_PD_Long) {
				this->longInvestor = investor;
				if (instrumentField.MaxMarketOrderVolume < YdPosition) {
					for (int p = YdPosition; p > 0; p-= limit) {
						std::this_thread::sleep_for(std::chrono::milliseconds(1000));
						makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Close, p > limit ? limit : p);
					}
				}
				else if (YdPosition > 0) {
					makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Close, YdPosition);
				}
			}
			else if (investor.PosiDirection == THOST_FTDC_PD_Short) {
				if (instrumentField.MaxMarketOrderVolume < YdPosition) {
					for (int p = YdPosition; p > 0; p-= limit) {
						std::this_thread::sleep_for(std::chrono::milliseconds(1000));
						makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_Close, p > limit ? limit : p);
					}
				}
				else if (YdPosition > 0) {
					makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_Close, YdPosition);
				}

			}
		}
		if (status & 2 != 0) {
			if (investor.PosiDirection == THOST_FTDC_PD_Long) {
				this->longInvestor = investor;
				if (instrumentField.MaxMarketOrderVolume < investor.TodayPosition) {
					for (int p = investor.TodayPosition; p > 0; p-= limit) {
						std::this_thread::sleep_for(std::chrono::milliseconds(1000));
						makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, p > limit ? limit : p);
					}

				}
				else if (investor.TodayPosition > 0) {
					makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, investor.TodayPosition);
				}
			}
			else if (investor.PosiDirection == THOST_FTDC_PD_Short) {
				shortInvestor = investor;
				if (instrumentField.MaxMarketOrderVolume < investor.TodayPosition) {
					for (int p = investor.TodayPosition; p > 0; p-= limit) {
						std::this_thread::sleep_for(std::chrono::milliseconds(1000));
						makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, p > limit ? limit : p);
					}
				}
				else if (investor.TodayPosition > 0) {
					makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, investor.TodayPosition);
				}

			}
		}

		if (!last) {
			initVolume = initVolume + investor.Position + investor.YdPosition;
		}
		if (isLast) {
			last = true;
		}
		MvStatus = 1;
	});
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
	double velocity = (pivotArray[0] - pivotArray[right]) / right;
	if (!isMin) {
		highPivotQue.push_back(std::pair<double, double>(pivotVal, velocity));
		outFile << "high:" << pivotVal<< std::endl;
	}
	else {
		lowPivotQue.push_back(std::pair<double, double>(pivotVal, velocity));
		outFile << "low:" << pivotVal << std::endl;
	}
	
	return pivotVal;
}
