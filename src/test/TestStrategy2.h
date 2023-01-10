#pragma once
#include "TestStrategy.h"
/*
class PivotReversalStrategy2 : public Strategy {
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
			<< "enter index" << ","
			<< "H/L" << "," << "status" << "," << "prestatus" << "," << "PivotPrice"
			<< std::endl;
		winFile.open(instrumentID + "_winRate.csv");
		winFile << "win"
			<< "," << "fail" << "," << "盈亏比" << "," << "盈利金额" << "," << "亏损金额" << std::endl;
		instrumentField.VolumeMultiple = 1000;
		this->instrumentField.PriceTick = 0.02;
	}

	~PivotReversalStrategy2() {
	}

	virtual void operator()() override;

	void makeOrder(double lastPrice, TThostFtdcDirectionType direction, TThostFtdcOffsetFlagType offsetFlag, TThostFtdcVolumeType volume);

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
				<< curPoint.lowIndx << "," << "L" << "," << status << "," << preStatus.load() << "," << curPoint.PivotPrice
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
				<< curPoint.highIndx << "," << "H" << "," << status << "," << preStatus.load() << "," << curPoint.PivotPrice << std::endl;
		}

		costArray.push_back(cost);
	}

	void subCurVolume(TThostFtdcVolumeType v, TThostFtdcDirectionType direction, double p) {
		curVolume -= v;
		double ps = v * (p*InstrumentCommissionRate.CloseTodayRatioByMoney *instrumentField.VolumeMultiple + InstrumentCommissionRate.CloseTodayRatioByVolume);
		double cost = v * p*instrumentField.VolumeMultiple - ps;
		if (preStatus == 2 || preStatus == 6) {
			cost = v * p*instrumentField.VolumeMultiple + ps;
			CommissionFile << instrumentID << ","
				<< v << ","
				<< p << ","
				<< ps << ","
				<< THOST_FTDC_OF_CloseToday << ","
				<< direction << ","
				<< cost << "," << curPoint.lowIndx << "," << "L" << "," << status << "," << preStatus.load() << "," << curPoint.PivotPrice << std::endl;
		}
		else {
			CommissionFile << instrumentID << ","
				<< v << ","
				<< p << ","
				<< ps << ","
				<< THOST_FTDC_OF_CloseToday << ","
				<< direction << ","
				<< cost << "," << curPoint.highIndx << "," << "H" << "," << status << "," << preStatus.load() << "," << curPoint.PivotPrice << std::endl;
		}

		costArray.push_back(cost);
		bool clear = false;
		if (curVolume > 0 && status - 8 < 3 && status - 8 >= 0) {
			clear = true;
		}

		if (curVolume == 0) {
			double sum = 0;
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
		else if (status - 8 > 3) {
			status = status - 8;
		}
		else if (!clear) {
			throw "error";
		}
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

	double getAvgWbVal() {

#ifdef MEStrategy
		if (presumProfit > 0) {
			double avg = curVolume * wbVal * 0.618 + presumProfit * 0.382;
			if (avg > curVolume * wbVal * 2) {
				avg = curVolume * wbVal * 2;
			}
			return avg;
		}
		else {
			return curVolume * wbVal;
		}
#else
		return curVolume * wbVal;
#endif
	}

	double getAvgFbVal() {
#ifdef MEStrategy
		if (presumLoss < 0) {
			double avg = curVolume * fbVal * 0.618 - presumLoss * 0.382;
			if (avg > curVolume * fbVal * 2) {
				avg = curVolume * fbVal * 2;
			}
			return avg;
		}
		else {
			return curVolume * fbVal;
		}
#else
		return curVolume * wbVal;
#endif
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
	std::atomic<int> status = 0; //
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
	double maxsum;
	int trendtimes = 0;
	std::atomic<int> curVolume = 0;
	CThostFtdcInstrumentField instrumentField;
	CThostFtdcInstrumentCommissionRateField InstrumentCommissionRate;
	double pivot(Strategy::Type type);
};
*/