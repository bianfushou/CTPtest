#include <vector>
#include <string>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <memory>
#include "StrategyTrade.h"
#include "CustomTradeSpi.h"

extern std::unordered_map<std::string, TickToKlineHelper> g_KlineHash;


// œﬂ≥Ãª•≥‚¡ø
std::mutex marketDataMutex;


void PivotReversalStrategy::makeOrder(double lastPrice, TThostFtdcDirectionType direction, TThostFtdcOffsetFlagType offsetFlag, TThostFtdcVolumeType volume) {
	std::shared_ptr<CThostFtdcInputOrderField> orderInsertReq = std::make_shared<CThostFtdcInputOrderField>();
	memset(orderInsertReq.get(), 0, sizeof(CThostFtdcInputOrderField));
	strcpy(orderInsertReq->InstrumentID, this->instrumentID.c_str());
	orderInsertReq->Direction = direction;
	orderInsertReq->CombOffsetFlag[0] = offsetFlag;
	orderInsertReq->LimitPrice = lastPrice;
	orderInsertReq->VolumeTotalOriginal = volume;
	orderInsertReq->StopPrice = 0;
	order_ref++;
	std::string ref = std::to_string(order_ref);
	ref = ref + "CN";
	strcpy_s(orderInsertReq->OrderRef, ref.c_str());
	OrderRefSet.insert(ref);
	if (canRemoveOrderSysID.size() > 20) {
		OrderSysIDSet.erase(canRemoveOrderSysID.front());
		canRemoveOrderSysID.pop_front();
	}
	if (canRemoveOrderSysID.size() > 20) {
		OrderRefSet.erase(canRemoveOrderRef.front());
		canRemoveOrderRef.pop_front();
	}
	if (offsetFlag == THOST_FTDC_OF_Open) {
		customTradeSpi->reqOrder(orderInsertReq);
		if (preStatus == 0) {
			if (direction == THOST_FTDC_D_Buy) {
				TickToKlineHelper& tickToKlineObject = g_KlineHash.at(this->instrumentID);
				curPoint.startTrade(lastPrice, highPivotQue.back(), trend, volume);
			}
			else if ((direction == THOST_FTDC_D_Sell)) {
				TickToKlineHelper& tickToKlineObject = g_KlineHash.at(this->instrumentID);
				curPoint.startTrade(lastPrice, lowPivotQue.back(), trend, volume);
			}
		}

	}
	else if(offsetFlag == THOST_FTDC_OF_CloseToday && getVolumeMatch()){
		if (curPoint.YDVolume <= volume && curPoint.YDVolume > 0) {
			orderInsertReq->VolumeTotalOriginal = curPoint.YDVolume;
			orderInsertReq->CombOffsetFlag[0] = THOST_FTDC_OF_Close;
		}
		customTradeSpi->reqOrder(orderInsertReq);
		curPoint.sellPrice = lastPrice;
	}
}
void PivotReversalStrategy::operator()()
{
	if (!opStart) {
		opStart = true;
		preStatus = 0;
		status = 0;
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
	if (highPivotQue.empty() && lowPivotQue.empty() && status > 0 && curVolume > 0) {
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
	double avglastPrice = tickToKlineObject.getNewMinPrice();
	if (swh < swl && tickToKlineObject.lastPrice > swh - this->instrumentField.PriceTick &&
		tickToKlineObject.lastPrice < swl + this->instrumentField.PriceTick) {
		if (swl - swh < 30 * this->instrumentField.PriceTick || fabs(tickToKlineObject.lastPrice - swl) < 
			10 * this->instrumentField.PriceTick || fabs(tickToKlineObject.lastPrice - swh) < this->instrumentField.PriceTick  * 10) {
			goto limit;
		}
		else if (status == 0) {
			curPoint.cross = true;
		}
	}
	if (fabs(swh - swl) <= this->instrumentField.PriceTick && fabs(swh - tickToKlineObject.lastPrice) <= this->instrumentField.PriceTick) {
		goto limit;
	}

	if (avglastPrice > swh && !highPivotQue.empty()) {
		if (status == 0) {
			if (curPoint.cross == false ||(trend == -1 && avglastPrice > swh + 10 * this->instrumentField.PriceTick)) {
				this->preStatus = 0;
				makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Open, volume);
				this->status = 8 | 1;
			}
		}
		else if (status == 2) {
			if (curPoint.cross == false || trend == -1) {
				preStatus = 2;
				{
					makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, curVolume.load());
					this->status = 8;
				}
				auto lastPrice = tickToKlineObject.lastPrice;
			}
		}
		else if (status == 6) {
			if (curPoint.cross == false || (trend == -1 && avglastPrice > swh + 10 * this->instrumentField.PriceTick)) {
				preStatus = 6;
				{
					makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, curVolume.load());
					this->status = 8;
				}
				auto lastPrice = tickToKlineObject.lastPrice;
			}
		}
		return;
	}
	if (avglastPrice < swl && !lowPivotQue.empty()) {
		if (status == 0) {
			if (curPoint.cross == false || (trend == 1 && avglastPrice < swl - 10 * this->instrumentField.PriceTick)) {
				preStatus = 0;
				makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_Open, volume);
				status = 8 | 2;
			}
		}
		else if (status == 1) {
			if (curPoint.cross == false || trend == 1) {
				this->preStatus = 1;
				{
					makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, curVolume.load());
					this->status = 8;
				}
				auto lastPrice = tickToKlineObject.lastPrice;
			}

		}
		else if (status == 5) {
			if (curPoint.cross == false || (trend == 1 && avglastPrice < swl - 10 * this->instrumentField.PriceTick)) {
				this->preStatus = 5;
				{
					makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, curVolume.load());
					this->status = 8;
				}
				auto lastPrice = tickToKlineObject.lastPrice;
			}
		}
		return;
	}
limit:
	if (status == 1) {
		double sum = sumCost(curVolume, tickToKlineObject.lastPrice);
		if (sum <= -2 * getAvgFbVal()) {
			this->preStatus = 1;


			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, curVolume.load());

			this->status = 8;
		}
		else if (sum >= 2 * getAvgWbVal()) {
			this->preStatus = 1;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, curVolume.load() / 2);
			this->status = 5 | 8;
		}
	}
	else if (status == 5) {
		double sum = sumCost(curVolume, tickToKlineObject.lastPrice);
		if (sum < wbVal * curVolume * 0.69) {
			this->preStatus = 5;
			{
				makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, curVolume.load());
				this->status = 8;
			}
		}
	}
	else if (status == 6) {
		double sum = sumCost(curVolume, tickToKlineObject.lastPrice);
		if (sum < wbVal * curVolume * 0.69) {
			this->preStatus = 6;
			{
				makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, curVolume.load());
				this->status = 8;
			}
		}
	}
	else if (status == 2) {
		double sum = sumCost(curVolume, tickToKlineObject.lastPrice);
		if (sum <= -2 * getAvgFbVal()) {
			this->preStatus = 2;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, curVolume.load());

			this->status = 8;
		}
		else if (sum >= 2 * getAvgWbVal()) {
			this->preStatus = 2;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, curVolume.load() / 2);
			this->status = 6 | 8;
		}
	}
}

void PivotReversalStrategy::clearInvestor(CThostFtdcInvestorPositionField investor, bool isLast) {
	PreSettlementPrice = investor.PreSettlementPrice;
	if (status >= 16) {
		if (isLast ) {
			if (status >= 16) {
				status = status - 16;
			}
		}
	}
	else if (status < 8 && status > 0 && getVolumeMatch() == false) {
		int YdPosition = investor.Position - investor.TodayPosition;
		if (investor.Position == curVolume) {
			if (investor.PosiDirection == THOST_FTDC_PD_Long && status == 1 && status == 5) {
				curPoint.YDVolume = YdPosition;
				curPoint.TDVolume = investor.TodayPosition;
				setVolumeMatch(true);
			}
			else if (investor.PosiDirection == THOST_FTDC_PD_Short && status == 2 && status == 6) {
				curPoint.YDVolume = YdPosition;
				curPoint.TDVolume = investor.TodayPosition;
				setVolumeMatch(true);
			}
		}
		if (isLast && getVolumeMatch() == false) {
			status = 0;
			curVolume = 0;
			outFile << "VolumeMatch Error " << "status:"<< status<<" curVolume:"<< curVolume << std::endl;
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
		trend = -1;
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
		trend = 1;
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
