#include <iostream>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include <fstream>
#include <windows.h>
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
	std::string left, right, volume, RatioByVolume;
	getConfig("Strategy", "LeftBars", left);
	getConfig("Strategy", "RightBars", right);
	getConfig("Strategy", "Volume", volume);
	getConfig("Strategy", "RatioByVolume", RatioByVolume);
	std::string P, B, WB, FB;
	getConfig("Strategy", "P", P);
	getConfig("Strategy", "B", B);
	getConfig("Strategy", "WB", WB);
	getConfig("Strategy", "FB", FB);
	auto pivotReversalStrategyPtr = std::make_shared<PivotReversalStrategy>();
	pivotReversalStrategyPtr->setLRBars(std::stoi(left), std::stoi(right));
	pivotReversalStrategyPtr->setRatioByVolume(std::stod(RatioByVolume));
	pivotReversalStrategyPtr->winRate(std::stod(P), std::stod(B), std::stod(WB), std::stod(FB));
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

std::string GetWorkingPath()
{
	char szFilePath[200 + 1] = { 0 };
	::GetModuleFileNameA(NULL, szFilePath, 200);

	std::string exePath(szFilePath);
	std::size_t pos = exePath.rfind('\\');
	if (pos != std::string::npos)
	{
		return exePath.substr(0, pos + 1);
	}
	return "";
}

void split(const std::string& s, const char& delim , std::vector<std::string>& tokens) {
	tokens.clear();
	size_t lastPos = s.find_first_not_of(delim, 0);
	size_t pos = s.find(delim, lastPos);
	while (lastPos != std::string::npos) {
		tokens.emplace_back(s.substr(lastPos, pos - lastPos));
		lastPos = s.find_first_not_of(delim, pos);
		pos = s.find(delim, lastPos);
	}
}

int main() {
	GetWorkingPath();
	initConfig();
	initStrategy();
	
	std::ifstream tickStream(testFileName);
	std::string title;
	std::getline(tickStream, title);
	std::string data;
	std::string InstrumentID(test_pTradeInstrumentID);
	CThostFtdcDepthMarketDataField dataField;
	std::vector<std::string> tokens;
	int i = 0;
	while (!tickStream.eof())
	{
		data.clear();
		std::getline(tickStream, data);
		i++;
		if (data.empty()) {
			break;
		}
		split(data, ',', tokens);
		dataField.LastPrice = std::stod(tokens[1]);
		dataField.Volume = std::stoi(tokens[2]);
		test_KlineHash[InstrumentID].KLineFromRealtimeData(&dataField);
		PivotReversalStrategy*  pivotStrategy = dynamic_cast<PivotReversalStrategy*>( test_StrategyMap[InstrumentID].get());
		//pivotStrategy->operator()();
		//pivotStrategy->operator()();
		pivotStrategy->improve();
		pivotStrategy->improve();
	}

	return 0;
}