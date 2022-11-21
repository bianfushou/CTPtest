#pragma once
// ---- 简单策略交易的类 ---- //

#include <functional>
#include "CTP_API/ThostFtdcUserApiStruct.h"
#include "TickToKlineHelper.h"
#include "CustomTradeSpi.h"

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
	void PivotReversalStrategy(TThostFtdcInstrumentIDType instrumentID, CustomTradeSpi *customTradeSpi);
private:

	double pivot(Strategy::Type type, int left, int right);
};

