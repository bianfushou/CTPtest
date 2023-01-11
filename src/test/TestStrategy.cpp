#include <vector>
#include <string>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <memory>
#include <iostream>
#include "TestStrategy.h"

extern std::unordered_map<std::string, TickToKlineHelper> test_KlineHash;
extern int gBarTimes;

void PivotReversalStrategy::makeOrder(double lastPrice, TThostFtdcDirectionType direction, TThostFtdcOffsetFlagType offsetFlag, TThostFtdcVolumeType volume, double PivotPrice) {

	std::shared_ptr<CThostFtdcInputOrderField> orderInsertReq = std::make_shared<CThostFtdcInputOrderField>();
	memset(orderInsertReq.get(), 0, sizeof(CThostFtdcInputOrderField));
	strcpy(orderInsertReq->InstrumentID, this->instrumentID.c_str());
	orderInsertReq->Direction = direction;
	orderInsertReq->CombOffsetFlag[0] = offsetFlag;
	orderInsertReq->LimitPrice = lastPrice;
	orderInsertReq->VolumeTotalOriginal = volume;
	orderInsertReq->StopPrice = 0;
	if (offsetFlag == THOST_FTDC_OF_Open) {
		
		if (preStatus == 0) {
			maxsum = 0;
			trendtimes = 0;
			limPrice = lastPrice;
			if (direction == THOST_FTDC_D_Buy) {
				TickToKlineHelper& tickToKlineObject = test_KlineHash.at(this->instrumentID);
				curPoint.startTrade(lastPrice, highPivotQue.back(), trend, PivotPrice);
				curPoint.highIndx = highPivotInd;
				curPoint.lowIndx = lowPivotInd;
			}
			else if ((direction == THOST_FTDC_D_Sell)) {
				TickToKlineHelper& tickToKlineObject = test_KlineHash.at(this->instrumentID);
				curPoint.startTrade(lastPrice, lowPivotQue.back(), trend, PivotPrice);
				curPoint.lowIndx = lowPivotInd;
				curPoint.highIndx = highPivotInd;
			}
		}
		addCurVolume(volume, direction, lastPrice);
	}
	else {
		curPoint.sellPrice = lastPrice;
		subCurVolume(volume, direction, lastPrice);
	}
}
void PivotReversalStrategy::operator()()
{
	if (!opStart) {
		opStart = true;
		curVolume = 0;
	}
	if (status >= 8) {
		std::cout << status << std::endl;
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
	if (swh <= 0.0) {
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
	TickToKlineHelper& tickToKlineObject = test_KlineHash.at(instrumentID);
	if (swh < swl && tickToKlineObject.lastPrice > swh - this->instrumentField.PriceTick && 
		tickToKlineObject.lastPrice < swl + this->instrumentField.PriceTick) {
		return;
	}
	if (fabs(swh - swl) <= this->instrumentField.PriceTick && fabs(swh - tickToKlineObject.lastPrice) <= this->instrumentField.PriceTick) {
		return;
	}
	
	if (tickToKlineObject.lastPrice > swh) {
		double hprice = 0;
		double lmhprice = 0;
		bool isCon = checkmarket(Strategy::Type::high, &hprice, &lmhprice);
		if (isCon) {
			if (tickToKlineObject.lastPrice <= hprice) {
				return;
			}
			else {
				swh = hprice;
			}
		}
		if (status == 0) {
			
			this->preStatus = 0;
			this->status = 8 | 1;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Open, volume, swh);
		}
		else if (status == 2) {
			preStatus = 2;
			{
				this->status = 8;
				makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, curVolume.load(), swh);
			}

		}
		else if (status == 1 && curVolume < volume) {
			this->preStatus = 1;
			this->status = 8 | 1;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Open, volume - curVolume,swh);
		}
		return;
	}
	if (tickToKlineObject.lastPrice < swl) {
		double lprice = 0;
		double lmlprice = 0;
		bool isCon = checkmarket(Strategy::Type::low, &lprice, &lmlprice);
		if (isCon) {
			if (tickToKlineObject.lastPrice >= lprice) {
				return;
			}
			else {
				swl = lprice;
			}
		}
		if (status == 0) {
			preStatus = 0;
			status = 8 | 2;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_Open, volume, swl);
		}
		else if (status == 1) {
			this->preStatus = 1;
			{
				this->status = 8;
				makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, curVolume.load(), swl);
			}
		}
		else if (status == 2 && curVolume < volume) {
			this->preStatus = 2;
			this->status = 8 | 2;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Open, volume - curVolume, swl);
		}
		return;
	}
	
	if (status == 1) {
		double sum = sumCost(curVolume, tickToKlineObject.lastPrice);
		if (maxsum < sum) {
			maxsum = sum;
		}
		if (limPrice < tickToKlineObject.lastPrice) {
			limPrice = tickToKlineObject.lastPrice;
		}
		if (limPrice < hAvgTimes + 1.382 * curPoint.PivotPrice  && tickToKlineObject.lastPrice < curPoint.PivotPrice - 2 * hAvgTimes) {
			this->preStatus = 2;
			this->status = 8;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, curVolume.load(), swl);
		}
		else if (tickToKlineObject.lastPrice < curPoint.PivotPrice - 3 * hAvgTimes && lowPivotQue.back() < curPoint.PivotPrice - 6 * hAvgTimes) {
			this->preStatus = 2;
			this->status = 8;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, curVolume.load(), swl);
		}
		if (limPrice > curPoint.PivotPrice + 8 * hAvgTimes && sum < 0.618 * maxsum) {
			this->preStatus = 1;
			this->status = 8;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, curVolume.load(), swl);
		}
	}
	else if (status == 2) {
		double sum = sumCost(curVolume, tickToKlineObject.lastPrice);
		if (maxsum < sum) {
			maxsum = sum;
		}
		if (limPrice > tickToKlineObject.lastPrice) {
			limPrice = tickToKlineObject.lastPrice;
		}
		if (limPrice > curPoint.PivotPrice - 1.382 * lAvgTimes && tickToKlineObject.lastPrice > curPoint.PivotPrice + 2 * lAvgTimes) {
			this->preStatus = 2;
			this->status = 8;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, curVolume.load(), swh);
		}
		else if (tickToKlineObject.lastPrice > curPoint.PivotPrice + 3 * lAvgTimes && highPivotQue.back() > curPoint.PivotPrice + 6 * lAvgTimes) {
			this->preStatus = 2;
			this->status = 8;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, curVolume.load(), swh);
		}
		if (limPrice < curPoint.PivotPrice - 8 * lAvgTimes&& sum < 0.618 * maxsum) {
			this->preStatus = 2;
			this->status = 8;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, curVolume.load(), swh);
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

		TickToKlineHelper& tickToKlineObject = test_KlineHash.at(instrumentID);
		if ((status & 1) != 0) {
			int YdPosition = investor.Position - investor.TodayPosition;
			if (investor.PosiDirection == THOST_FTDC_PD_Long) {
				this->longInvestor = investor;
				if (instrumentField.MaxMarketOrderVolume < YdPosition) {
					for (int p = YdPosition; p > 0; p -= limit) {
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
					for (int p = YdPosition; p > 0; p -= limit) {
						std::this_thread::sleep_for(std::chrono::milliseconds(1000));
						makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_Close, p > limit ? limit : p);
					}
				}
				else if (YdPosition > 0) {
					makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_Close, YdPosition);
				}

			}
		}
		if ((status & 2) != 0) {
			if (investor.PosiDirection == THOST_FTDC_PD_Long) {
				this->longInvestor = investor;
				if (instrumentField.MaxMarketOrderVolume < investor.TodayPosition) {
					for (int p = investor.TodayPosition; p > 0; p -= limit) {
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
					for (int p = investor.TodayPosition; p > 0; p -= limit) {
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
	TickToKlineHelper& tickToKlineObject = test_KlineHash.at(instrumentID);
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
				if (pivotVal <= pivotArray[i] || fabs(pivotVal - pivotArray[i]) < 0.0005) {
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
	int pretrend = trend;
	if (!isMin) {
		highPivotInd ++;
		highPivotQue.push_back(pivotVal);
		outFile << "high:" << pivotVal << std::endl;
		trend = -1;
		if (highPivotQue.size() > 100) {
			highPivotQue.pop_front();
		}
		if (highPivotInd < 50) {
			hAvgTimes = (hAvgTimes * (highPivotInd - 1) + pivotVal - pivotArray[0]) / highPivotInd;
		}
		else {
			hAvgTimes = (hAvgTimes * 49 + +pivotVal - pivotArray[0]) / 50;
		}
	}
	else {
		lowPivotQue.push_back(pivotVal);
		outFile << "low:" << pivotVal << std::endl;
		if (lowPivotQue.size() > 100) {
			lowPivotQue.pop_front();
		}
		trend = 1;
		lowPivotInd ++;
		if (lowPivotInd < 50) {
			lAvgTimes = (lAvgTimes * (lowPivotInd - 1) + pivotArray[0] - pivotVal) / lowPivotInd;
		}
		else {
			lAvgTimes = (lAvgTimes * 49 + pivotArray[0] - pivotVal) / 50;
		}
	}
	if (trend != pretrend) {
		trendtimes++;
	}
	return pivotVal;
}
bool PivotReversalStrategy::checkmarket(Strategy::Type type, double *p, double* lp) {
	std::vector<double> pivotArray;
	
	switch(type){
		case Strategy::Type::high:
		{
			if(highPivotQue.size() > 2){
				int i = 2;
				for (auto it = highPivotQue.rbegin(); i > 0; it++, i--) {
					pivotArray.push_back(*it);
				}

				*p = std::max(pivotArray[0], pivotArray[1]);
				*lp = std::min(pivotArray[0], pivotArray[1]);
				if (fabs(pivotArray[0] - pivotArray[1]) <= hAvgTimes * 2) {
					if (fabs(pivotArray[0] - pivotArray[1]) <= hAvgTimes && pivotArray[0] < pivotArray[1]) {
						*p = (pivotArray[0] + pivotArray[1]) / 2 + hAvgTimes;
					}
					else if(fabs(pivotArray[0] - pivotArray[1]) <= hAvgTimes){
						*p = *lp + hAvgTimes;
					}
					return true;
				}
				
			}
			else {
				*p = highPivotQue.back() + hAvgTimes;
				*lp = highPivotQue.back();
				return true;
			}
			break;
		}
		case Strategy::Type::low:
		{
			if (lowPivotQue.size() > 2) {
				int i = 2;
				for (auto it = lowPivotQue.rbegin(); i > 0; it++, i--) {
					pivotArray.push_back(*it);
				}

				*p = std::min(pivotArray[0], pivotArray[1]);
				*lp = std::max(pivotArray[0], pivotArray[1]);
				if (fabs(pivotArray[0] - pivotArray[1]) <= lAvgTimes * 2) {
					if (fabs(pivotArray[0] - pivotArray[1]) <= lAvgTimes && pivotArray[0] > pivotArray[1]) {
						*p = (pivotArray[0] + pivotArray[1]) / 2 - lAvgTimes;
					}
					else if (fabs(pivotArray[0] - pivotArray[1]) <= lAvgTimes) {
						*p = *lp - lAvgTimes;
					}
					return true;
				}

			}
			else {
				*p = lowPivotQue.back() - hAvgTimes;
				*lp = lowPivotQue.back();
				return true;
			}
			break;
		}
	}
	return false;
}