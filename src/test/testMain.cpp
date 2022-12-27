#include <iostream>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include "../CTP_API/ThostFtdcUserApiStruct.h"
#include "../TickToKlineHelper.h"
#include "TestStrategy.h"




// ---- ȫ�ֱ��� ---- //

// �������
std::unordered_map<std::string, TickToKlineHelper> g_KlineHash;              // ��ͬ��Լ��k�ߴ洢��

// ���ײ���
TThostFtdcInstrumentIDType g_pTradeInstrumentID;        // �����׵ĺ�Լ����

std::unordered_map<std::string, std::shared_ptr<Strategy>> g_StrategyMap;
int gBarTimes;

void initStrategy() {
	std::string left, right, volume;
	getConfig("Strategy", "LeftBars", left);
	getConfig("Strategy", "RightBars", right);
	getConfig("Strategy", "Volume", volume);
	auto pivotReversalStrategyPtr = std::make_shared<PivotReversalStrategy>();
	pivotReversalStrategyPtr->setLRBars(std::stoi(left), std::stoi(right));
	g_StrategyMap.emplace(g_pTradeInstrumentID, pivotReversalStrategyPtr);
	g_StrategyMap[g_pTradeInstrumentID]->setInstrument(g_pTradeInstrumentID);
	g_StrategyMap[g_pTradeInstrumentID]->setVolume(std::stoi(volume));
	g_StrategyMap[g_pTradeInstrumentID]->init();
}

void initConfig() {
	std::string instrumentID, barTimes;

	getConfig("config", "InstrumentID", instrumentID);
	strcpy_s(g_pTradeInstrumentID, instrumentID.c_str());

	getConfig("config", "BarTimes", barTimes);
	gBarTimes = std::stoi(barTimes);
}

int main() {
	return 0;
}