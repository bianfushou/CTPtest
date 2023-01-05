#pragma once
// ---- �򵥲��Խ��׵��� ---- //

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

	~PivotReversalStrategy() {
		if (pivotfile) {
			fclose(pivotfile);
		}
	}
	virtual void init() override {
		outFile.open(instrumentID + "_Strategy.txt");
		CommissionFile.open(instrumentID + "_Commission.csv");
		CommissionFile << "��Լ����" << ","
			<< "����" << ","
			<< "�۸�" << ","
			<< "������" << ","
			<< "��ƽ�ֱ�־" << ","
			<< "��ƽ�ַ���" << ","
			<< "ӯ�����"
			<< std::endl;
		winFile.open(instrumentID + "_winRate.csv");
		winFile << "win"
			<< "," << "fail" << "," << "ӯ����" << "," << "ӯ�����" << "," << "������" << std::endl;
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
		preStatus = 0;
		status = 16;
		curVolume = 0;
	}

	void stop() {
		status = 16;
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

	void clearInvestor(CThostFtdcInvestorPositionField investor, bool isLast);

	void addCurVolume(TThostFtdcVolumeType v, TThostFtdcDirectionType direction, double p) {
		std::lock_guard<std::mutex> lk(strategyMutex);
		curVolume += v;
		double ps = v * (p*InstrumentCommissionRate.OpenRatioByMoney *instrumentField.VolumeMultiple + InstrumentCommissionRate.OpenRatioByVolume);
		double cost = -(v * instrumentField.VolumeMultiple * p + ps);
		CommissionFile << instrumentID << ","
			<< v << ","
			<< p << ","
			<< ps << ","
			<< THOST_FTDC_OF_Open << ","
			<< direction << ","
			<< cost << std::endl;
		if (status >= 8) {
			status -= 8;
		}

		costArray.push_back(cost);
	}

	void subCurVolume(TThostFtdcVolumeType v, TThostFtdcDirectionType direction, double p) {
		std::lock_guard<std::mutex> lk(strategyMutex);
		if (status >= 16) {
			return;
		}
		curVolume -= v;
		double ps = v * (p*InstrumentCommissionRate.CloseTodayRatioByMoney *instrumentField.VolumeMultiple + InstrumentCommissionRate.CloseTodayRatioByVolume);
		double cost = v * instrumentField.VolumeMultiple * p - ps;
		CommissionFile << instrumentID << ","
			<< v << ","
			<< p << ","
			<< ps<< ","
			<< THOST_FTDC_OF_CloseToday << ","
			<< direction << ","
			<< cost <<std::endl;
		costArray.push_back(cost);
		bool clear = false;
		if (curVolume > 0 && status - 8 < 3 && status - 8 >= 0) {
			makeClearOrder(0, direction, THOST_FTDC_OF_CloseToday, curVolume.load());
			clear = true;
		}
		if(curVolume == 0){
			double sum = 0;
			for (double cs : costArray) {
				sum += cs;
			}

			if (sum > 0) {
				profit += sum;
				winNum++;
				if (loss != 0)
					winFile << winNum << "," << faillNum << "," << profit / loss << "," << profit << "," << loss << std::endl;
				else {
					winFile << winNum << "," << faillNum << "," << "N/A" << "," << profit << "," << loss << std::endl;
				}
			}
			else {
				faillNum++;
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
		else if (status - 8 > 3) {
			status = status - 8;
		}
		else if(!clear){
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

	void setInstrumentCommissionRate(CThostFtdcInstrumentCommissionRateField commissionRate) {
		InstrumentCommissionRate = commissionRate;
	}
	
	double curCost(TThostFtdcVolumeType v, double p) {
		double ps = v * (p*InstrumentCommissionRate.CloseTodayRatioByMoney *instrumentField.VolumeMultiple + InstrumentCommissionRate.CloseTodayRatioByVolume);
		return v * p * instrumentField.VolumeMultiple - ps;
	}

	double sumCost(TThostFtdcVolumeType v, double p) {
		double cost = curCost(v, p);
		double sum = 0;
		for (double cs : costArray) {
			sum += cs;
		}
		return sum + cost;
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

	CThostFtdcInvestorPositionField longInvestor;
	CThostFtdcInvestorPositionField shortInvestor;
	std::atomic<int> preStatus = 0;
	std::atomic<int> status = 0; //0�޵���1��൥�� 2��յ�
	int left;
	int right;
	int barsNumHigh = 0;
	int barsNumLow = 0;

	std::atomic<int> initVolume = 0;
	bool last = false;
	bool opStart = false;
	int MvStatus = 0;
	std::list<std::function<void()>> taskQue;
	std::vector<std::thread> tasks;
	std::vector<double> costArray;
	double profit = 0;
	double loss = 0;
	std::atomic<int> curVolume = 0;
	CThostFtdcInstrumentField instrumentField;
	double PreSettlementPrice = 0;
	CThostFtdcInstrumentCommissionRateField InstrumentCommissionRate;
	double pivot(Strategy::Type type);
};