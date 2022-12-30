#include <iostream>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include "../CTP_API/ThostFtdcUserApiStruct.h"
#include "../TickToKlineHelper.h"
#include "TestStrategy.h"




// ---- 全局变量 ---- //

// 行情参数
std::unordered_map<std::string, TickToKlineHelper> test_KlineHash;              // 不同合约的k线存储表

// 交易参数
TThostFtdcInstrumentIDType test_pTradeInstrumentID;        // 所交易的合约代码

std::unordered_map<std::string, std::shared_ptr<Strategy>> test_StrategyMap;
int gBarTimes;
std::string testFileName;

void initStrategy() {
	std::string left, right, volume;
	getConfig("Strategy", "LeftBars", left);
	getConfig("Strategy", "RightBars", right);
	getConfig("Strategy", "Volume", volume);
	auto pivotReversalStrategyPtr = std::make_shared<PivotReversalStrategy>();
	pivotReversalStrategyPtr->setLRBars(std::stoi(left), std::stoi(right));
	test_StrategyMap.emplace(test_pTradeInstrumentID, pivotReversalStrategyPtr);
	test_StrategyMap[test_pTradeInstrumentID]->setInstrument(test_pTradeInstrumentID);
	test_StrategyMap[test_pTradeInstrumentID]->setVolume(std::stoi(volume));
	test_StrategyMap[test_pTradeInstrumentID]->init();
}

void initConfig() {
	std::string instrumentID, barTimes, fileName;

	getConfig("config", "InstrumentID", instrumentID);
	strcpy_s(test_pTradeInstrumentID, instrumentID.c_str());

	getConfig("config", "BarTimes", barTimes);
	gBarTimes = std::stoi(barTimes);

	getConfig("config", "FileName", fileName);
	testFileName = fileName;

	test_KlineHash[instrumentID] = TickToKlineHelper();
	test_KlineHash[instrumentID].isRecord = true;
	test_KlineHash[instrumentID].instrument = instrumentID;
}

int main() {
	initConfig();
	initStrategy();
	
	return 0;
}