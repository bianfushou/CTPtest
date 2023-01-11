#pragma once
// ---- 简单策略交易的类 ---- //

#include"../Config.h"
#include <functional>
#ifdef CTPTest
#include "../CTPTest_API/ThostFtdcUserApiStruct.h"
#else
#include "../CTP_API/ThostFtdcUserApiStruct.h"
#endif
#include "../TickToKlineHelper.h"
#include <list>
#include <fstream>
#include <mutex>
#include <atomic>
#include <algorithm>
class Strategy {
public:
	enum Type {
		open, high, low, close
	};

	void setInstrument(TThostFtdcInstrumentIDType instrumentId) {
		instrumentID = std::string(instrumentId);
	}

	void setVolume(TThostFtdcVolumeType volume) {
		const_cast<TThostFtdcVolumeType&>(this->volume) = volume;
	}

	virtual void init() = 0;

	virtual void operator()() = 0;

	virtual void start() {
		tradeStart = true;
	}

	bool getTradeStart() {
		return tradeStart;
	}
protected:
	std::string instrumentID;
	const TThostFtdcVolumeType volume = 1;
	bool tradeStart = false;
};


struct TradePoint {
	double price;
	double PivotPrice;
	double firstTurnPrice;
	int trend;
	int startVolume;
	int firstTurnVolume;
	int curVolume;
	double sellPrice;
	bool isTurn = false;
	int highIndx = 0;
	int lowIndx = 0;

	void startTrade(double price, double PivotPrice, int trend, int startVolume) {
		clear();
		this->price = price;
		this->PivotPrice = PivotPrice;
		this->trend = trend;
		this->startVolume = startVolume;
	}

	bool cmpTrend(int t) {
		return t == trend;
	}

	void setFirst(double firstTurnPrice, int firstTurnVolume) {
		if (!isTurn) {
			isTurn = true;
			this->firstTurnPrice = firstTurnPrice;
			this->firstTurnVolume = firstTurnVolume;
		}
	}
	void clear() {
		isTurn = false;
		this->price = 0;
		this->PivotPrice = 0;
		this->trend = 0;
		this->startVolume = 0;
	}

	void setCurVolume(int curVolume) {
		this->curVolume = curVolume;
	}
};

struct ShakeTable {
	void clear() { isLow = false; isHigh = false; }
	void setHigh(std::vector<double>& highvec, double hAvgTimes) {
		high = *std::max_element(highvec.cbegin(), highvec.cend());
		high += 2 * hAvgTimes;
		isHigh = true;
	}

	void setLow(std::vector<double>& lowvec, double lAvgTimes) {
		low = *std::max_element(lowvec.cbegin(), lowvec.cend());
		low += 2 * lAvgTimes;
		isLow = true;
	}
private:
	double high;
	double low;
	bool isHigh = false;
	bool isLow = false;
};

class PivotReversalStrategy : public Strategy {
public:
	enum Type {
		open, high, low, close
	};

	void setLRBars(int left, int right) {
		this->left = left;
		this->right = right;
	}

	void setRatioByVolume(double ratioByVolume) {
		InstrumentCommissionRate.CloseTodayRatioByVolume = ratioByVolume;
	}
	virtual void init() override {
		outFile.open(instrumentID + "_Strategy.txt");
		CommissionFile.open(instrumentID + "_Commission.csv");
		CommissionFile << "合约代码" << ","
			<< "手数" << ","
			<< "价格" << ","
			<< "手续费" << ","
			<< "开平仓标志" << ","
			<< "开平仓方向" << ","
			<< "盈利金额" << ","
			<<"enter index" << ","
			<<"H/L" <<","<<"status"<<","<<"prestatus"<<","<<"PivotPrice"<<","<<"Win/Fail"
			<< std::endl;
		winFile.open(instrumentID + "_winRate.csv");
		winFile << "win"
			<< "," <<"fail"<<","<< "盈亏比"<<","<< "盈利金额"<<","<< "亏损金额"<< std::endl;
		instrumentField.VolumeMultiple = 1000;
		this->instrumentField.PriceTick = 0.02;
	}

	~PivotReversalStrategy() {
	}

	virtual void operator()() override;

	void improve();

	void resetStatus() {
		taskQue.clear();
		status.store(preStatus.load());
	}
	void statusDone() {
		if (status >= 8) {
			status -= 8;
		}
	}

	virtual void start() {
		Strategy::start();
		taskQue.clear();
		preStatus = 0;
		status = 0;
	}
	
	void stop() {
		status = 16;
	}
	int getStatus() {
		return status;
	}

	void makeOrder(double lastPrice, TThostFtdcDirectionType direction, TThostFtdcOffsetFlagType offsetFlag, TThostFtdcVolumeType volume);

	void makeClearOrder(double lastPrice, TThostFtdcDirectionType direction, TThostFtdcOffsetFlagType offsetFlag, TThostFtdcVolumeType volume) {
		std::shared_ptr<CThostFtdcInputOrderField> orderInsertReq = std::make_shared<CThostFtdcInputOrderField>();
		memset(orderInsertReq.get(), 0, sizeof(CThostFtdcInputOrderField));
		strcpy(orderInsertReq->InstrumentID, this->instrumentID.c_str());
		orderInsertReq->Direction = direction;
		orderInsertReq->CombOffsetFlag[0] = offsetFlag;
		orderInsertReq->LimitPrice = 0;
		orderInsertReq->VolumeTotalOriginal = volume;
		//orderInsertReq->StopPrice = 0;
		///报单引用
		//strcpy(orderInsertReq.OrderRef, order_ref);
		///报单价格条件: 限价
		orderInsertReq->OrderPriceType = THOST_FTDC_OPT_AnyPrice;
		///组合投机套保标志
		orderInsertReq->CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
		///有效期类型: 当日有效
		orderInsertReq->TimeCondition = THOST_FTDC_TC_IOC;
		///成交量类型: 任何数量
		orderInsertReq->VolumeCondition = THOST_FTDC_VC_AV;
		///最小成交量: 1
		orderInsertReq->MinVolume = 1;
		///触发条件: 立即
		orderInsertReq->ContingentCondition = THOST_FTDC_CC_Immediately;
		///强平原因: 非强平
		orderInsertReq->ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
		///自动挂起标志: 否
		orderInsertReq->IsAutoSuspend = 0;
		///用户强评标志: 否
		//orderInsertReq.UserForceClose = 0;
	}

	void makeClearLimitOrder(double lastPrice, TThostFtdcDirectionType direction, TThostFtdcOffsetFlagType offsetFlag, TThostFtdcVolumeType volume) {
		std::shared_ptr<CThostFtdcInputOrderField> orderInsertReq = std::make_shared<CThostFtdcInputOrderField>();
		memset(orderInsertReq.get(), 0, sizeof(CThostFtdcInputOrderField));
		strcpy(orderInsertReq->InstrumentID, this->instrumentID.c_str());
		orderInsertReq->Direction = direction;
		orderInsertReq->CombOffsetFlag[0] = offsetFlag;
		orderInsertReq->LimitPrice = lastPrice;
		orderInsertReq->VolumeTotalOriginal = volume;
		//orderInsertReq->StopPrice = 0;
		///报单引用
		//strcpy(orderInsertReq.OrderRef, order_ref);
		///报单价格条件: 限价
		orderInsertReq->OrderPriceType = THOST_FTDC_OPT_LimitPrice;
		///组合投机套保标志
		orderInsertReq->CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
		///有效期类型: 当日有效
		orderInsertReq->TimeCondition = THOST_FTDC_TC_IOC;
		///成交量类型: 任何数量
		orderInsertReq->VolumeCondition = THOST_FTDC_VC_AV;
		///最小成交量: 1
		orderInsertReq->MinVolume = 1;
		///触发条件: 立即
		orderInsertReq->ContingentCondition = THOST_FTDC_CC_Immediately;
		///强平原因: 非强平
		orderInsertReq->ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
		///自动挂起标志: 否
		orderInsertReq->IsAutoSuspend = 0;
		///用户强评标志: 否
		//orderInsertReq.UserForceClose = 0;
	}

	void clearInvestor(CThostFtdcInvestorPositionField investor, int status, bool isLast);

	void addCurVolume(TThostFtdcVolumeType v, TThostFtdcDirectionType direction, double p) {
		curVolume += v;
		double ps = v * (p*InstrumentCommissionRate.OpenRatioByMoney *instrumentField.VolumeMultiple + InstrumentCommissionRate.OpenRatioByVolume);
		double cost = -(v * p * instrumentField.VolumeMultiple + ps);
		
		
		if (status >= 8) {
			status -= 8;
		}
		if (status == 2) {
			cost = -(v * p * instrumentField.VolumeMultiple - ps);
			CommissionFile << instrumentID << ","
				<< v << ","
				<< p << ","
				<< ps << ","
				<< THOST_FTDC_OF_Open << ","
				<< direction << ","
				<< cost << ","
				<< curPoint.lowIndx << "," <<"L" << "," << status<< "," << preStatus.load() << "," << curPoint.PivotPrice << "," << "N"
				<<std::endl;
		}
		else {
			CommissionFile << instrumentID << ","
				<< v << ","
				<< p << ","
				<< ps << ","
				<< THOST_FTDC_OF_Open << ","
				<< direction << ","
				<< cost << ","
				<< curPoint.highIndx << "," <<"H" << "," << status << "," << preStatus.load() << "," << curPoint.PivotPrice << "," << "N" << std::endl;
		}
		
		costArray.push_back(cost);
	}

	void subCurVolume(TThostFtdcVolumeType v, TThostFtdcDirectionType direction, double p) {
		curVolume -= v;
		double ps = v * (p*InstrumentCommissionRate.CloseTodayRatioByMoney *instrumentField.VolumeMultiple + InstrumentCommissionRate.CloseTodayRatioByVolume);
		double cost = v * p*instrumentField.VolumeMultiple - ps;
		
		costArray.push_back(cost);
		bool clear = false;
		if (curVolume > 0 && status - 8 < 3 && status - 8 >= 0) {
			makeClearOrder(0, direction, THOST_FTDC_OF_CloseToday, curVolume.load());
			clear = true;
		}
		double sum = 0;
		if (curVolume == 0) {
			
			for (double cs : costArray) {
				sum += cs;
			}
			if (preStatus == 2 || preStatus == 6) {
				sum = -sum;
			}

			if (sum > 0) {
				profit += sum;
				presumProfit = sum;
				winNum++;
				if (loss != 0)
					winFile << winNum << "," << faillNum<<"," << profit / loss <<","<< profit << ","<<loss<< std::endl;
				else {
					winFile << winNum << "," << faillNum << "," << "N/A" << "," << profit << "," << loss << std::endl;
				}
			}
			else {
				faillNum++;
				presumLoss = sum;
				loss += (-sum);
				if (loss != 0)
					winFile <<winNum << "," << faillNum << "," << profit / loss << "," << profit << "," << loss << std::endl;
				else {
					winFile << winNum << "," << faillNum << "," << "N/A" << "," << profit << "," << loss << std::endl;
				}
			}
			costArray.clear();
			status = 0;
			maxsum = 0;
		}
		else if (status - 8 > 0) {
			status = status - 8;
		}
		else if (!clear) {
			throw "error";
		}
		std::string win = "N";
		if (sum > 0) {
			win = "W";
		}
		else {
			win = "F";
		}
		if (preStatus == 2 || preStatus == 6) {
			cost = v * p*instrumentField.VolumeMultiple + ps;
			CommissionFile << instrumentID << ","
				<< v << ","
				<< p << ","
				<< ps << ","
				<< THOST_FTDC_OF_CloseToday << ","
				<< direction << ","
				<< cost << "," << curPoint.lowIndx << "," << "L" << "," << status << "," << preStatus.load() << "," << curPoint.PivotPrice<<","<<win << std::endl;
		}
		else {
			CommissionFile << instrumentID << ","
				<< v << ","
				<< p << ","
				<< ps << ","
				<< THOST_FTDC_OF_CloseToday << ","
				<< direction << ","
				<< cost << "," << curPoint.highIndx << "," << "H" << "," << status << "," << preStatus.load() << "," << curPoint.PivotPrice << "," << win << std::endl;
		}
	}

	bool getOpStart() {
		return opStart;
	}

	void setInstrumentField(const CThostFtdcInstrumentField& field) {
		if (strcmp(field.InstrumentID, instrumentID.c_str()) == 0) {
			instrumentField = field;
		}
	}

	void setInstrumentCommissionRate(CThostFtdcInstrumentCommissionRateField commissionRate) {
		InstrumentCommissionRate = commissionRate;
	}
	void clearStatus(int v) {
		if (MvStatus == 1) {
			initVolume -= v;
			//customTradeSpi->reqQueryInvestorPosition();
		}
	}

	double curCost(TThostFtdcVolumeType v, double p) {
		double ps = v * (p*InstrumentCommissionRate.CloseTodayRatioByMoney *instrumentField.VolumeMultiple + InstrumentCommissionRate.CloseTodayRatioByVolume);
		if (status == 2 || status == 6) {
			return v * p*instrumentField.VolumeMultiple + ps;
		}
		else {
			return v * p * instrumentField.VolumeMultiple - ps;
		}
	}

	double sumCost(TThostFtdcVolumeType v, double p) {
		double cost = curCost(v, p);
		double sum = 0;
		for (double cs : costArray) {
			sum += cs;
		}
		if (status == 2 || status == 6) {
			return -(sum + cost);
		}
		else {
			return sum + cost;
		}
	}

	void winRate(double pVal, double bVal, double wbVal, double fbVal) {
		this->pVal = pVal;
		this->bVal = bVal;
		this->wbVal = wbVal;
		this->fbVal = fbVal;
	}

	double getAvgWbVal() {
		return curVolume * wbVal;
	}

	double getAvgFbVal() {
		return curVolume * wbVal;
	}

	double getPivotSplit() {
		double pivotSplit = 0;
		if (!highPivotQue.empty()) {
			pivotSplit = highPivotQue.back();
		}
		else {
			pivotSplit = 10 * this->instrumentField.PriceTick;
		}

		if (lowPivotQue.size() > 0) {
			pivotSplit -= lowPivotQue.back();
		}
		else {
			pivotSplit -= 10 * this->instrumentField.PriceTick;
			if (pivotSplit < 0) {
				pivotSplit = -pivotSplit;
			}
			if (pivotSplit < 10 * this->instrumentField.PriceTick) {
				pivotSplit = 10 * this->instrumentField.PriceTick;
			}
		}
		return pivotSplit;
	}

	bool checkmarket(Strategy::Type type, double *p, double* lp);
private:
	std::ofstream outFile;
	std::ofstream CommissionFile;
	std::ofstream winFile;
	std::mutex strategyMutex;
	std::list<double> highPivotQue;
	double hAvgTimes = 0;
	std::atomic<int> highPivotInd = 0;
	std::list<double> lowPivotQue;
	double lAvgTimes = 0;
	std::atomic<int> lowPivotInd = 0;

	CThostFtdcInvestorPositionField longInvestor;
	CThostFtdcInvestorPositionField shortInvestor;
	std::atomic<int> preStatus = 0;
	std::atomic<int> status = 0; //0无单，1买多单， 2买空单
	int left;
	int right;
	int barsNumHigh = 0;
	int barsNumLow = 0;

	std::vector<TradePoint> pointVec;
	TradePoint curPoint;

	double pVal = 0;
	double bVal = 0;
	double wbVal = 0;
	double fbVal = 0;

	int winNum = 0;
	int faillNum = 0;
	double presumProfit = 0;
	double presumLoss = 0;
	int trend = 0;
	int initVolume = 0;
	bool last = false;
	bool opStart = false;
	int MvStatus = 0;
	std::list<std::function<void()>> taskQue;
	std::vector<std::thread> tasks;
	std::vector<double> costArray;
	double profit = 0;
	double loss = 0;
	double maxsum = 0;
	double limPrice = 0;
	int trendtimes = 0;
	std::atomic<int> curVolume = 0;
	CThostFtdcInstrumentField instrumentField;
	CThostFtdcInstrumentCommissionRateField InstrumentCommissionRate;
	double pivot(Strategy::Type type);
};