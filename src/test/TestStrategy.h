#pragma once
// ---- �򵥲��Խ��׵��� ---- //

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

class Strategy {
public:
	enum Type {
		open, high, low, close
	};

	void setInstrument(TThostFtdcInstrumentIDType instrumentId) {
		instrumentID = std::string(instrumentId);
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
	TThostFtdcVolumeType volume = 1;
	bool tradeStart = false;
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
			<< "," <<"fail"<<","<< "ӯ����" << std::endl;
	}

	~PivotReversalStrategy() {
	}

	virtual void operator()() override;

	void resetStatus() {
		taskQue.clear();
		status = preStatus;
	}
	void statusDone() {
		if (status >= 8) {
			status -= 8;
		}
	}

	int getStatus() {
		return status;
	}

	void makeOrder(double lastPrice, TThostFtdcDirectionType direction, TThostFtdcOffsetFlagType offsetFlag, TThostFtdcVolumeType volume) {
		std::shared_ptr<CThostFtdcInputOrderField> orderInsertReq = std::make_shared<CThostFtdcInputOrderField>();
		memset(orderInsertReq.get(), 0, sizeof(CThostFtdcInputOrderField));
		strcpy(orderInsertReq->InstrumentID, this->instrumentID.c_str());
		orderInsertReq->Direction = direction;
		orderInsertReq->CombOffsetFlag[0] = offsetFlag;
		orderInsertReq->LimitPrice = lastPrice;
		orderInsertReq->VolumeTotalOriginal = volume;
		orderInsertReq->StopPrice = 0;
		if (offsetFlag == THOST_FTDC_OF_Open) {
			addCurVolume(volume, direction, lastPrice);
		}
		else {
			subCurVolume(volume, direction, lastPrice);
		}
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
	}

	void clearInvestor(CThostFtdcInvestorPositionField investor, int status, bool isLast);

	void addCurVolume(TThostFtdcVolumeType v, TThostFtdcDirectionType direction, double p) {
		curVolume += v;
		double ps = v * (p*InstrumentCommissionRate.OpenRatioByMoney *instrumentField.VolumeMultiple + InstrumentCommissionRate.OpenRatioByVolume);
		double cost = -(v * p + ps);
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
		curVolume -= v;
		double ps = v * (p*InstrumentCommissionRate.CloseTodayRatioByMoney *instrumentField.VolumeMultiple + InstrumentCommissionRate.CloseTodayRatioByVolume);
		double cost = v * p - ps;
		CommissionFile << instrumentID << ","
			<< v << ","
			<< p << ","
			<< ps << ","
			<< THOST_FTDC_OF_CloseToday << ","
			<< direction << ","
			<< cost << std::endl;
		costArray.push_back(cost);
		if (curVolume > 0) {
			makeClearOrder(0, direction, THOST_FTDC_OF_CloseToday, curVolume.load());
		}
		else if (curVolume == 0) {
			double sum = 0;
			for (double cs : costArray) {
				sum += cs;
			}

			if (sum > 0) {
				profit += sum;
				winNum++;
				if (loss != 0)
					winFile << winNum << "," << faillNum<<"," << profit / loss << std::endl;
				else {
					winFile << winNum << "," << faillNum << "," << "N/A" << std::endl;
				}
			}
			else {
				faillNum++;
				loss += (-sum);
				if (loss != 0)
					winFile <<winNum << "," << faillNum << "," << profit / loss << std::endl;
				else {
					winFile << winNum << "," << faillNum << "," << "N/A" << std::endl;
				}
			}
			costArray.clear();
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

	void setInstrumentCommissionRate(CThostFtdcInstrumentCommissionRateField commissionRate) {
		InstrumentCommissionRate = commissionRate;
	}
	void clearStatus(int v) {
		if (MvStatus == 1) {
			initVolume -= v;
			//customTradeSpi->reqQueryInvestorPosition();
		}
	}

private:
	std::ofstream outFile;
	std::ofstream CommissionFile;
	std::ofstream winFile;
	std::mutex strategyMutex;
	std::list<double> highPivotQue;
	std::list<double> lowPivotQue;

	CThostFtdcInvestorPositionField longInvestor;
	CThostFtdcInvestorPositionField shortInvestor;
	int preStatus = 0;
	int status = 0; //0�޵���1��൥�� 2��յ�
	int left;
	int right;
	int barsNumHigh = 0;
	int barsNumLow = 0;

	int winNum = 0;
	int faillNum = 0;

	int initVolume = 0;
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
	CThostFtdcInstrumentCommissionRateField InstrumentCommissionRate;
	double pivot(Strategy::Type type);
};