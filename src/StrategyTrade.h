#pragma once
// ---- 简单策略交易的类 ---- //

#include"Config.h"
#include <functional>
#include<stdio.h>
#ifdef CTPTest
#include "CTPTest_API/ThostFtdcUserApiStruct.h"
#else
#include "CTP_API/ThostFtdcUserApiStruct.h"
#endif
#include "TickToKlineHelper.h"
#include "CustomTradeSpi.h"
#include <list>
#include <fstream>
#include <mutex>
#include <atomic>
#include <set>

extern int gBarTimes;

class Strategy {
public:
	enum Type {
		open, high, low, close
	};

	void setInstrument(TThostFtdcInstrumentIDType instrumentId, CustomTradeSpi *tradeSpi) {
		instrumentID = std::string(instrumentId);
		customTradeSpi = tradeSpi;
	}

	void setVolume(TThostFtdcVolumeType volume) {
		this->volume = volume;
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
	CustomTradeSpi *customTradeSpi;
	TThostFtdcVolumeType volume = 1;
	bool tradeStart = false;
};

struct TradePoint {
	bool cross = false;
	double price;
	double PivotPrice;
	double firstTurnPrice;
	int trend;
	int startVolume;
	int firstTurnVolume;
	int curVolume;
	double maxPrice;
	double sellPrice;
	bool isTurn = false;
	double turnPrice = 0;
	double limPrice = 0;
	std::atomic<bool> isHalf = false;
	int highIndx = 0;
	int lowIndx = 0;
	int YDVolume = 0;
	int TDVolume = 0;

	void startTrade(double price, double PivotPrice, int trend, int startVolume) {
		clear();
		this->price = price;
		limPrice = price;
		this->PivotPrice = PivotPrice;
		this->trend = trend;
		this->startVolume = startVolume;
		maxPrice = price;
	}

	bool cmpTrend(int t) {
		return t == trend;
	}

	void setTurn(bool turn, double price) {
		isTurn = turn;
		turnPrice = price;
	}
	bool getTurn() {
		return isTurn;
	}
	void clear() {
		isTurn = false;
		this->price = 0;
		this->PivotPrice = 0;
		this->trend = 0;
		this->startVolume = 0;
		this->cross = false;
		maxPrice = 0;
		isHalf = false;
		limPrice = 0;
		YDVolume = 0;
		TDVolume = 0;
	}

	void setCurVolume(int curVolume) {
		this->curVolume = curVolume;
	}
};

class PivotReversalStrategy: public Strategy {
public:

	TThostFtdcFrontIDType	trade_front_id;
	TThostFtdcSessionIDType	session_id;
	int	order_ref = 10;
	std::set<std::string>	OrderSysIDSet;
	std::set<std::string>	OrderRefSet;
	std::list<std::string> canRemoveOrderSysID;
	std::list<std::string> canRemoveOrderRef;

	bool isMyOrder(TThostFtdcFrontIDType FrontID, TThostFtdcSessionIDType SessionID, TThostFtdcOrderRefType OrderRef)
	{
		return ((FrontID == trade_front_id) &&
			(SessionID == session_id) &&
			(OrderRefSet.find(OrderRef) != OrderRefSet.end()));
	}

	bool isMyOrder(TThostFtdcOrderRefType OrderRef)
	{
		return (OrderRefSet.find(OrderRef) != OrderRefSet.end());
	}

	enum Type {
		open, high, low, close
	};

	void setLRBars(int left, int right) {
		this->left = left;
		this->right = right;
	}

	~PivotReversalStrategy() {
		if (pivotfile) {
			fclose(pivotfile);
		}
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
			<< "enter index" << ","
			<< "H/L" << "," << "status" << "," << "prestatus" << "," << "PivotPrice" << "," << "Win/Fail"
			<< std::endl;
		winFile.open(instrumentID + "_winRate.csv");
		winFile << "win"
			<< "," << "fail" << "," << "盈亏比" << "," << "盈利金额" << "," << "亏损金额" << std::endl;
		std::string PivotMin = instrumentID + "PivotMin.con";
		pivotfile = fopen(PivotMin.c_str(), "rb+");
		if (pivotfile) {
			double time[3];
			int size = fread(time, sizeof(double), 3, pivotfile);
			if (size == 4) {
				time_t t_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
				time_t t_h;
				int ts = fread(&t_h, sizeof(time_t), 1, pivotfile);
				if (ts == 1 && t_c - t_h  < 10800 && abs(t_c - t_h) < 10800) {
					if (time[0] > time[1] && time[1] > 0 && fabs(gBarTimes - time[2]) < 0.5) {
						highPivotQue.push_back(time[0]);
						lowPivotQue.push_back(time[1]);
					}
				}
			}
		}
		else {
			pivotfile = fopen(PivotMin.c_str(), "wb+");
		}
	}
	virtual void operator()() override;

	void resetStatus() {
		std::lock_guard<std::mutex> lk(strategyMutex);
		taskQue.clear();
		status.store(preStatus.load());
	}
	void statusDone() {
		std::lock_guard<std::mutex> lk(strategyMutex);
		if (status >= 8) {
			status -= 8;
		}
	}

	int getStatus() {
		return status;
	}

	virtual void start() {
		Strategy::start();
		taskQue.clear();
	}

	void stop() {
		status = 16 | status;
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
		customTradeSpi->reqOrder(orderInsertReq, false);
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
		customTradeSpi->reqOrder(orderInsertReq, false);
	}

	void clearInvestor(CThostFtdcInvestorPositionField investor, bool isLast);

	void addCurVolume(TThostFtdcVolumeType v, TThostFtdcDirectionType direction, double p) {
		std::lock_guard<std::mutex> lk(strategyMutex);
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
				<< curPoint.lowIndx << "," << "L" << "," << status << "," << preStatus.load() << "," << curPoint.PivotPrice << "," << "N"
				<< std::endl;
		}
		else {
			CommissionFile << instrumentID << ","
				<< v << ","
				<< p << ","
				<< ps << ","
				<< THOST_FTDC_OF_Open << ","
				<< direction << ","
				<< cost << ","
				<< curPoint.highIndx << "," << "H" << "," << status << "," << preStatus.load() << "," << curPoint.PivotPrice << "," << "N" << std::endl;
		}
		curPoint.isHalf = false;
		costArray.push_back(cost);
	}

	void subNeedCloseVolume(TThostFtdcVolumeType v, TThostFtdcDirectionType direction, double p) {
		std::lock_guard<std::mutex> lk(strategyMutex);
		if (status >= 16) {
			needcloseVolume -= v;
			if (needcloseVolume == 0) {
				if (status >= 16) {
					status = status - 16;
				}
			}
		}
	}

	void subCurVolume(TThostFtdcVolumeType v, TThostFtdcDirectionType direction, double p) {
		std::lock_guard<std::mutex> lk(strategyMutex);
		if (status >= 16) {
			return;
		}
		curVolume -= v;
		if (curPoint.YDVolume > 0 && curPoint.YDVolume >= v) {
			curPoint.YDVolume -= v;
		}
		double ps = v * (p*InstrumentCommissionRate.CloseTodayRatioByMoney *instrumentField.VolumeMultiple + InstrumentCommissionRate.CloseTodayRatioByVolume);
		double cost = v * instrumentField.VolumeMultiple * p - ps;
		if (preStatus == 2 || preStatus == 6) {
			cost = v * p*instrumentField.VolumeMultiple + ps;
		}
		costArray.push_back(cost);
		double sum = 0;
		if(curVolume == 0){
			
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
					winFile << winNum << "," << faillNum << "," << profit / loss << "," << profit << "," << loss << std::endl;
				else {
					winFile << winNum << "," << faillNum << "," << "N/A" << "," << profit << "," << loss << std::endl;
				}
			}
			else {
				faillNum++;
				presumLoss = sum;
				loss += (-sum);
				if (loss != 0)
					winFile << winNum << "," << faillNum << "," << profit / loss << "," << profit << "," << loss << std::endl;
				else {
					winFile << winNum << "," << faillNum << "," << "N/A" << "," << profit << "," << loss << std::endl;
				}
			}
			costArray.clear();
			status = 0;
		}
		else if (curVolume > 0 && preStatus > 0 && preStatus < 8) {
			status.store(preStatus);
		}
		else {
			outFile << "ERR:" << curVolume <<" status:"<< status<< " prestatus:"<< preStatus <<std::endl;
		}

		if(status == 0){
			std::string win = "N";
			if (sum > 0) {
				win = "W";
			}
			else {
				win = "F";
			}
			if (preStatus == 2 || preStatus == 6) {
				CommissionFile << instrumentID << ","
					<< v << ","
					<< p << ","
					<< ps << ","
					<< THOST_FTDC_OF_CloseToday << ","
					<< direction << ","
					<< cost << "," << curPoint.lowIndx << "," << "L" << "," << status << "," << preStatus.load() << "," << curPoint.PivotPrice << "," << win << std::endl;
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

	void setCurVolume(int curVol) {
		this->curVolume = curVol;
	}

	double getAvgWbVal() {
		return curVolume * wbVal;
	}

	double getAvgFbVal() {
		return curVolume * wbVal;
	}

	bool getVolumeMatch() {
		return volumeMatch;
	}

	void setVolumeMatch(bool match) {
		volumeMatch = volumeMatch;
	}
private:
	std::ofstream outFile;
	std::ofstream CommissionFile;
	std::ofstream winFile;
	FILE* pivotfile = nullptr;
	std::mutex strategyMutex;
	std::list<double> highPivotQue;
	std::list<double> lowPivotQue;

	double pVal = 0;
	double bVal = 0;
	double wbVal = 0;
	double fbVal = 0;
	int winNum = 0;
	int faillNum = 0;
	std::atomic<int> needcloseVolume = 0;
	CThostFtdcInvestorPositionField longInvestor;
	CThostFtdcInvestorPositionField shortInvestor;
	std::atomic<int> preStatus = 0;
	std::atomic<int> status = 0; //0无单，1买多单， 2买空单
	int left;
	int right;
	int barsNumHigh = 0;
	int barsNumLow = 0;
	double presumProfit = 0;
	double presumLoss = 0;
	double maxSum = 0;

	TradePoint curPoint;
	bool volumeMatch = true;

	std::atomic<int> initVolume = 0;
	bool last = false;
	bool opStart = false;
	int MvStatus = 0;
	std::list<std::function<void()>> taskQue;
	std::vector<std::thread> tasks;
	std::vector<double> costArray;
	double profit = 0;
	double loss = 0;
	int trend = 0;
	std::atomic<int> curVolume = 0;
	CThostFtdcInstrumentField instrumentField;
	double PreSettlementPrice = 0;
	CThostFtdcInstrumentCommissionRateField InstrumentCommissionRate;
	double pivot(Strategy::Type type);
};