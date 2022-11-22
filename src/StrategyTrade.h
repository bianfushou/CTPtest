#pragma once
// ---- 简单策略交易的类 ---- //

#include <functional>
#include "CTP_API/ThostFtdcUserApiStruct.h"
#include "TickToKlineHelper.h"
#include "CustomTradeSpi.h"
#include <list>
#include <mutex>

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

	void setLRBars(int left, int right) {
		this->left = left;
		this->right = right;
	}

	void PivotReversalStrategy();
private:
	std::string instrumentID;
	CustomTradeSpi *customTradeSpi;
	std::mutex dataMutex;
	std::list<double> highPivotQue;
	std::list<double> lowPivotQue;
	int left;
	int right;
	double pivot(Strategy::Type type);
};

