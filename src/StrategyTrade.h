#pragma once
// ---- �򵥲��Խ��׵��� ---- //

#include"Config.h"
#include <functional>
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

typedef void(*reqOrderInsertFun)(
	TThostFtdcInstrumentIDType instrumentID,
	TThostFtdcPriceType price,
	TThostFtdcVolumeType volume,
	TThostFtdcDirectionType direction);

using ReqOrderInsertFunctionType = std::function<
	void(TThostFtdcInstrumentIDType instrumentID,
	TThostFtdcPriceType price,
	TThostFtdcVolumeType volume,
	TThostFtdcDirectionType direction)>;

void StrategyCheckAndTrade(TThostFtdcInstrumentIDType instrumentID, CustomTradeSpi *customTradeSpi);

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

	void start() {
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


class PivotReversalStrategy: public Strategy {
public:
	enum Type {
		open, high, low, close
	};

	void setLRBars(int left, int right) {
		this->left = left;
		this->right = right;
	}
	virtual void init() override {
		outFile.open(instrumentID + "_Strategy.txt");
	}
	virtual void operator()() override;

	void resetStatus() {
		std::lock_guard<std::mutex> lk(strategyMutex);
		taskQue.clear();
		status = preStatus;
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

	void makeOrder(double lastPrice, TThostFtdcDirectionType direction, TThostFtdcOffsetFlagType offsetFlag, TThostFtdcVolumeType volume ) {
		std::shared_ptr<CThostFtdcInputOrderField> orderInsertReq = std::make_shared<CThostFtdcInputOrderField>();
		memset(orderInsertReq.get(), 0, sizeof(CThostFtdcInputOrderField));
		strcpy(orderInsertReq->InstrumentID, this->instrumentID.c_str());
		orderInsertReq->Direction = direction;
		orderInsertReq->CombOffsetFlag[0] = offsetFlag;
		orderInsertReq->LimitPrice = lastPrice;
		orderInsertReq->VolumeTotalOriginal = volume;
		orderInsertReq->StopPrice = 0;
		customTradeSpi->reqOrder(orderInsertReq);
	}

	void makeClearOrder(double lastPrice, TThostFtdcDirectionType direction, TThostFtdcOffsetFlagType offsetFlag, TThostFtdcVolumeType volume) {
		std::shared_ptr<CThostFtdcInputOrderField> orderInsertReq = std::make_shared<CThostFtdcInputOrderField>();
		memset(orderInsertReq.get(), 0, sizeof(CThostFtdcInputOrderField));
		strcpy(orderInsertReq->InstrumentID, this->instrumentID.c_str());
		orderInsertReq->Direction = direction;
		orderInsertReq->CombOffsetFlag[0] = offsetFlag;
		orderInsertReq->LimitPrice = 0;
		orderInsertReq->VolumeTotalOriginal = volume;
		//orderInsertReq->StopPrice = 0;
		///��������
		//strcpy(orderInsertReq.OrderRef, order_ref);
		///�����۸�����: �޼�
		orderInsertReq->OrderPriceType = THOST_FTDC_OPT_AnyPrice;
		///���Ͷ���ױ���־
		orderInsertReq->CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
		///��Ч������: ������Ч
		orderInsertReq->TimeCondition = THOST_FTDC_TC_IOC;
		///�ɽ�������: �κ�����
		orderInsertReq->VolumeCondition = THOST_FTDC_VC_AV;
		///��С�ɽ���: 1
		orderInsertReq->MinVolume = 1;
		///��������: ����
		orderInsertReq->ContingentCondition = THOST_FTDC_CC_Immediately;
		///ǿƽԭ��: ��ǿƽ
		orderInsertReq->ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
		///�Զ������־: ��
		orderInsertReq->IsAutoSuspend = 0;
		///�û�ǿ����־: ��
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
		///��������
		//strcpy(orderInsertReq.OrderRef, order_ref);
		///�����۸�����: �޼�
		orderInsertReq->OrderPriceType = THOST_FTDC_OPT_LimitPrice;
		///���Ͷ���ױ���־
		orderInsertReq->CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
		///��Ч������: ������Ч
		orderInsertReq->TimeCondition = THOST_FTDC_TC_IOC;
		///�ɽ�������: �κ�����
		orderInsertReq->VolumeCondition = THOST_FTDC_VC_AV;
		///��С�ɽ���: 1
		orderInsertReq->MinVolume = 1;
		///��������: ����
		orderInsertReq->ContingentCondition = THOST_FTDC_CC_Immediately;
		///ǿƽԭ��: ��ǿƽ
		orderInsertReq->ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
		///�Զ������־: ��
		orderInsertReq->IsAutoSuspend = 0;
		///�û�ǿ����־: ��
		//orderInsertReq.UserForceClose = 0;
		customTradeSpi->reqOrder(orderInsertReq, false);
	}

	void clearInvestor(CThostFtdcInvestorPositionField investor, int status, bool isLast);

	void addCurVolume(TThostFtdcVolumeType v) {
		std::lock_guard<std::mutex> lk(strategyMutex);
		curVolume += v;
		if (status >= 8) {
			status -= 8;
		}
	}

	void subCurVolume(TThostFtdcVolumeType v, TThostFtdcDirectionType direction, TThostFtdcOffsetFlagType offsetFlag) {
		std::lock_guard<std::mutex> lk(strategyMutex);
		curVolume -= v;
		if (curVolume > 0) {
			makeClearOrder(0, direction, offsetFlag, curVolume.load());
		}
		else if(curVolume == 0){
			status = 0;
		}
		else {
			throw "error";
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
	void clearStatus(int v) {
		if (MvStatus == 1) {
			initVolume -= v;
			//customTradeSpi->reqQueryInvestorPosition();
		}
	}
	
private:
	std::ofstream outFile;
	std::mutex strategyMutex;
	std::list<std::pair<double, double>> highPivotQue;
	std::list< std::pair<double, double>> lowPivotQue;

	CThostFtdcInvestorPositionField longInvestor;
	CThostFtdcInvestorPositionField shortInvestor;
	int preStatus = 0;
	int status = 0; //0�޵���1��൥�� 2��յ�
	int left;
	int right;
	int barsNumHigh = 0;
	int barsNumLow = 0;

	int initVolume = 0;
	bool last = false;
	bool opStart = false;
	int MvStatus = 0;
	std::list<std::function<void()>> taskQue;
	std::vector<std::thread> tasks;
	std::atomic<int> curVolume = 0;
	CThostFtdcInstrumentField instrumentField;
	double pivot(Strategy::Type type);
};