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
	if (highPivotQue.empty() && lowPivotQue.empty() && status > 0) {
		TickToKlineHelper& tickToKline = g_KlineHash.at(instrumentID);
		if (status == 2 || status == 6) {
			preStatus.store( status.load());
			{
				makeOrder(tickToKline.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, curVolume.load());
				this->status = 8;
			}
		}
		else if(status == 1 || status == 5){
			preStatus.store(status.load());
			{
				makeOrder(tickToKline.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, curVolume.load());
				this->status = 8;
			}
		}
	}
	if (swh <= 0.0 ) {
		if (!highPivotQue.empty()) {
			swh = highPivotQue.back();
		}
	}

	if (swl <= 0.0) {
		if (!lowPivotQue.empty()) {
			swl = lowPivotQue.back();
		}
	}
	if (swh <= 0.0 && swl <= 0.0) {
		return;
	}
	TickToKlineHelper& tickToKlineObject = g_KlineHash.at(instrumentID);
	if (tickToKlineObject.lastPrice > swh && !highPivotQue.empty()) {
		if (status == 0) {
			this->preStatus = 0;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Open, volume);
			
			this->status = 8 | 1;
			return;
		}
		else if (status == 2) {
			preStatus = 2;
			{
				makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, curVolume.load());
				this->status = 8;
			}
			auto lastPrice = tickToKlineObject.lastPrice;
			taskQue.emplace_back([this, swh](){
				this->preStatus = 0;
				std::lock_guard<std::mutex> lk(this->strategyMutex);
				TickToKlineHelper& tickToKlineObject = g_KlineHash.at(this->instrumentID);
#ifdef MEStrategy
				double Sprice = tickToKlineObject.lastPrice;
				if (tickToKlineObject.lastPriceArray.size() >= 6) {
					Sprice = tickToKlineObject.lastPriceArray.front();
				}
				if (tickToKlineObject.lastPrice > swh + 2.2 * this->instrumentField.PriceTick || (tickToKlineObject.lastPrice > swh && tickToKlineObject.lastPrice - Sprice >= 2 * this->instrumentField.PriceTick)) {
#else
				if (tickToKlineObject.lastPrice > swh) {
#endif
					makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Open, volume);
					this->status = 8 | 1;
				}
				else {
					this->status = 0;
				}
				
			});
			return;
			
		}
		else if (status == 6) {
			preStatus = 6;
			{
				makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, curVolume.load());
				this->status = 8;
			}
			auto lastPrice = tickToKlineObject.lastPrice;
			taskQue.emplace_back([this, swh]() {
				this->preStatus = 0;
				std::lock_guard<std::mutex> lk(strategyMutex);
				TickToKlineHelper& tickToKlineObject = g_KlineHash.at(this->instrumentID);
				if (tickToKlineObject.lastPrice > swh) {
					makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Open, volume);
					this->status = 8 | 1;
				}
				else {
					this->status = 0;
				}

			});
			return;
		}
	}
	if (tickToKlineObject.lastPrice < swl && !lowPivotQue.empty()) {
		if (status == 0) {
			preStatus = 0;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_Open, volume);
			status = 8 | 2;
			return;
		}
		else if (status == 1) {
			this->preStatus = 1;
			{
				makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, curVolume.load());
				this->status = 8;
			}
			auto lastPrice = tickToKlineObject.lastPrice;
			taskQue.emplace_back([this, swl]() {
				std::lock_guard<std::mutex> lk(strategyMutex);
				this->preStatus = 0;
				TickToKlineHelper& tickToKlineObject = g_KlineHash.at(this->instrumentID);
#ifdef MEStrategy
				double Sprice = tickToKlineObject.lastPrice;
				if (tickToKlineObject.lastPriceArray.size() >= 6) {
					Sprice = tickToKlineObject.lastPriceArray.front();
				}
				if (tickToKlineObject.lastPrice < swl - 2.2 * this->instrumentField.PriceTick || (tickToKlineObject.lastPrice < swl && tickToKlineObject.lastPrice - Sprice <= -2 * this->instrumentField.PriceTick)) {
#else
				if (tickToKlineObject.lastPrice < swl) {
#endif
					makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_Open, volume);
					this->status = 8 | 2;
				}
				else {
					this->status = 0;
				}
			});
			return;

		}
		else if (status == 5) {
			this->preStatus = 5;
			{
				makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, curVolume.load());
				this->status = 8;
			}
			auto lastPrice = tickToKlineObject.lastPrice;
			taskQue.emplace_back([this, swl]() {
				std::lock_guard<std::mutex> lk(strategyMutex);
				this->preStatus = 0;
				TickToKlineHelper& tickToKlineObject = g_KlineHash.at(this->instrumentID);
				if (tickToKlineObject.lastPrice < swl) {
					makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_Open, volume);
					this->status = 8 | 2;
				}
				else {
					this->status = 0;
				}
			});
			return;
		}
	}
	if (status == 1) {
		double sum = sumCost(curVolume, tickToKlineObject.lastPrice);
#ifdef MEStrategy
		double pivotSplit = highPivotQue.back();
		if (lowPivotQue.size() > 0) {
			pivotSplit -= lowPivotQue.back();
		}
		else{
			pivotSplit -= PreSettlementPrice;
			if (pivotSplit < 0) {
				pivotSplit = 0.618 * highPivotQue.back();
			}
		}
		pivotSplit = highPivotQue.back() - 0.618 * pivotSplit;
		if (sum <= -(fbVal* curVolume) || (sum < 0 && tickToKlineObject.lastPrice < pivotSplit)) {
#else
		if (sum <= -(fbVal* curVolume)) {
#endif
			this->preStatus = 1;


			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, curVolume.load());

			this->status = 8;
		}
		else if (sum >= wbVal * curVolume) {
			this->preStatus = 1;
#ifdef MEStrategy
			if (tickToKlineObject.lastPrice < highPivotQue.back()) {
				makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, curVolume.load() / 2 + curVolume.load() % 2);
			}
#else
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, curVolume.load() / 2);
#endif
			this->status = 5 | 8;
		}
		else if (curVolume < volume && tickToKlineObject.lastPrice > swh) {
			this->preStatus = 1;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Open, volume - curVolume);
			this->status = 8 | 1;
		}
	}
	else if (status == 5) {
		double sum = sumCost(curVolume, tickToKlineObject.lastPrice);
		if (sum < wbVal * curVolume / 2) {
			this->preStatus = 5;
			{
				makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, curVolume.load());
				this->status = 8;
			}
		}
	}
	else if (status == 6) {
		double sum = sumCost(curVolume, tickToKlineObject.lastPrice);
		if (sum < wbVal * curVolume / 2) {
			this->preStatus = 6;
			{
				makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, curVolume.load());
				this->status = 8;
			}
		}
	}
	else if (status == 2) {
		double sum = sumCost(curVolume, tickToKlineObject.lastPrice);
#ifdef MEStrategy
		double pivotSplit = -lowPivotQue.back();
		if (highPivotQue.size() > 0) {
			pivotSplit += highPivotQue.back();
		}
		else {
			pivotSplit += PreSettlementPrice;
			if (pivotSplit < 0) {
				pivotSplit = 0.618 * lowPivotQue.back();
			}
		}
		pivotSplit = lowPivotQue.back() + 0.618 * pivotSplit;
		if (sum <= -(fbVal* curVolume) || (sum < 0 && tickToKlineObject.lastPrice > pivotSplit)) {
#else
		if (sum <= -(fbVal* curVolume)) {
#endif
			this->preStatus = 2;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, curVolume.load());

			this->status = 8;
		}
		else if (sum >= wbVal * curVolume) {
			this->preStatus = 2;
#ifdef MEStrategy
			if (tickToKlineObject.lastPrice > lowPivotQue.back()) {
				makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, curVolume.load() / 2 + curVolume.load() % 2);
			}
#else
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, curVolume.load() / 2);
#endif
			this->status = 6 | 8;
		}
		else if (curVolume < volume && tickToKlineObject.lastPrice < swl) {
			this->preStatus = 2;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_Open, volume - curVolume);
			this->status = 8 | 2;
		}
	}
}

void PivotReversalStrategy::clearInvestor(CThostFtdcInvestorPositionField investor, bool isLast) {
	PreSettlementPrice = investor.PreSettlementPrice;
	if (status >= 16) {
		tasks.emplace_back([this, investor, isLast]() {
			std::lock_guard<std::mutex> lk(strategyMutex);
			int limit = instrumentField.MaxMarketOrderVolume / 3;
			limit = limit > instrumentField.MinMarketOrderVolume ? limit : instrumentField.MinMarketOrderVolume;

			TickToKlineHelper& tickToKlineObject = g_KlineHash.at(instrumentID);
			int YdPosition = investor.Position - investor.TodayPosition;
			if (investor.PosiDirection == THOST_FTDC_PD_Long) {
				this->longInvestor = investor;
				if (instrumentField.MaxMarketOrderVolume < YdPosition) {
					for (int p = YdPosition; p > 0; p -= limit) {
						makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_Close, p > limit ? limit : p);
						std::this_thread::sleep_for(std::chrono::milliseconds(2000));
					}
				}
				else if (YdPosition > 0) {
					makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_Close, YdPosition);
				}
			}
			else if (investor.PosiDirection == THOST_FTDC_PD_Short) {
				if (instrumentField.MaxMarketOrderVolume < YdPosition) {
					for (int p = YdPosition; p > 0; p -= limit) {
						makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Close, p > limit ? limit : p);
						std::this_thread::sleep_for(std::chrono::milliseconds(2000));
					}
				}
				else if (YdPosition > 0) {
					makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Close, YdPosition);
				}

			}

			if (investor.TodayPosition > volume) {
				if (investor.PosiDirection == THOST_FTDC_PD_Long) {
					if (this->status == 16) {
						for (int p = investor.TodayPosition - volume; p > 0; p -= limit) {
							makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, p > limit ? limit : p);
							std::this_thread::sleep_for(std::chrono::milliseconds(2000));
						}
						this->status = (1 | 16);
						this->setCurVolume(volume);
						this->costArray.push_back(-volume*investor.SettlementPrice*instrumentField.VolumeMultiple +investor.PositionProfit);
					}
					else if (this->status > 16) {
						for (int p = investor.TodayPosition; p > 0; p -= limit) {
							makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, p > limit ? limit : p);
							std::this_thread::sleep_for(std::chrono::milliseconds(2000));
						}
					}
				}
				else if (investor.PosiDirection == THOST_FTDC_PD_Short) {
					if (this->status == 16) {
						for (int p = investor.TodayPosition - volume; p > 0; p -= limit) {
							makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, p > limit ? limit : p);
							std::this_thread::sleep_for(std::chrono::milliseconds(2000));
						}
						this->status = (2 | 16);
						this->setCurVolume(volume);
						this->costArray.push_back(-volume * investor.SettlementPrice*instrumentField.VolumeMultiple + investor.PositionProfit);
					}
					else if(this->status > 16){
						for (int p = investor.TodayPosition; p > 0; p -= limit) {
							makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, p > limit ? limit : p);
							std::this_thread::sleep_for(std::chrono::milliseconds(2000));
						}
					}
				}
			}
			else if(investor.TodayPosition < volume){
				if (investor.PosiDirection == THOST_FTDC_PD_Long) {
					if (this->status == 16) {
						this->status = (5 | 16);
						this->setCurVolume(investor.TodayPosition);
						this->costArray.push_back(-investor.TodayPosition* investor.SettlementPrice*instrumentField.VolumeMultiple + investor.PositionProfit);
					}
					else {
						for (int p = investor.TodayPosition; p > 0; p -= limit) {
							makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, p > limit ? limit : p);
							std::this_thread::sleep_for(std::chrono::milliseconds(2000));
						}
					}
					
				}
				else if (investor.PosiDirection == THOST_FTDC_PD_Short) {
					if (this->status == 16) {
						this->status = (6 | 16);
						this->setCurVolume(investor.TodayPosition);
						this->costArray.push_back(-investor.TodayPosition* investor.SettlementPrice*instrumentField.VolumeMultiple + investor.PositionProfit);
					}
					else {
						for (int p = investor.TodayPosition; p > 0; p -= limit) {
							makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, p > limit ? limit : p);
							std::this_thread::sleep_for(std::chrono::milliseconds(2000));
						}
					}
				}
			}
			else {
				if (investor.PosiDirection == THOST_FTDC_PD_Long) {
					if (this->status == 16) {
						this->status = (1 | 16);
						this->setCurVolume(volume);
						this->costArray.push_back(-investor.TodayPosition* investor.SettlementPrice*instrumentField.VolumeMultiple + investor.PositionProfit);
					}
					else {
						for (int p = investor.TodayPosition; p > 0; p -= limit) {
							makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, p > limit ? limit : p);
							std::this_thread::sleep_for(std::chrono::milliseconds(2000));
						}
					}
				}
				else if (investor.PosiDirection == THOST_FTDC_PD_Short) {
					if (this->status == 16) {
						this->status = (2 | 16);
						this->setCurVolume(volume);
						this->costArray.push_back(-investor.TodayPosition* investor.SettlementPrice*instrumentField.VolumeMultiple + investor.PositionProfit);
					}
					else
					{
						for (int p = investor.TodayPosition; p > 0; p -= limit) {
							makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, p > limit ? limit : p);
							std::this_thread::sleep_for(std::chrono::milliseconds(2000));
						}
					}
				}
			}
			if (isLast) {
				if (status >= 16) {
					status = status - 16;
				}
			}
		});
	}

}

double PivotReversalStrategy::pivot(Strategy::Type type) {
	int range = left + right;
	std::vector<double> pivotArray;
	TickToKlineHelper& tickToKlineObject = g_KlineHash.at(instrumentID);
	if (tickToKlineObject.m_KLineDataArray.size() < range) {
		return 0.0;
	}

	size_t size = tickToKlineObject.kData;
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
		outFile << "H:" << pivotVal<< std::endl;
		if (highPivotQue.size() > 20) {
			highPivotQue.pop_front();
		}
		if (lowPivotQue.size() > 0 && pivotfile) {
			double timePivot[3];
			fseek(pivotfile, 0, SEEK_SET);
			timePivot[0] = pivotVal;
			timePivot[1] = lowPivotQue.back();
			timePivot[2] = gBarTimes;
			fwrite(timePivot, sizeof(double), 3, pivotfile);
			time_t t_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			fwrite(&t_c, sizeof(time_t), 1, pivotfile);
		}
		
	}
	else {
		lowPivotQue.push_back(pivotVal);
		outFile << "L:" << pivotVal << std::endl;
		if (lowPivotQue.size() > 20) {
			lowPivotQue.pop_front();
		}
		if (highPivotQue.size() > 0 && pivotfile) {
			double timePivot[3];
			fseek(pivotfile, 0, SEEK_SET);
			timePivot[0] = highPivotQue.back();
			timePivot[1] = pivotVal;
			timePivot[2] = gBarTimes;
			fwrite(timePivot, sizeof(double), 3, pivotfile);
			time_t t_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
			fwrite(&t_c, sizeof(time_t), 1, pivotfile);
		}
	}
	
	return pivotVal;
}
