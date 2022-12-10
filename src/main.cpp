#include <iostream>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include "CustomMdSpi.h"
#include "CustomTradeSpi.h"
#include "TickToKlineHelper.h"
#include "StrategyTrade.h"

using namespace std;

// 链接库
#pragma comment (lib, "thostmduserapi_se.lib")
#pragma comment (lib, "thosttraderapi_se.lib")

// ---- 全局变量 ---- //
// 公共参数
TThostFtdcBrokerIDType gBrokerID;                         // 模拟经纪商代码
TThostFtdcInvestorIDType gInvesterID;                         // 投资者账户名
TThostFtdcPasswordType gInvesterPassword;                     // 投资者密码

// 行情参数
CThostFtdcMdApi *g_pMdUserApi = nullptr;                           // 行情指针
//180.168.146.187:10211
//180.168.146.187:10212
//218.202.237.33:10213
//180.168.146.187:10131
char gMdFrontAddr[100];               // 模拟行情前置地址
char *g_pInstrumentID[1]; // 行情合约代码列表，中、上、大、郑交易所各选一种
int instrumentNum = 1;                                             // 行情合约订阅数量
unordered_map<string, TickToKlineHelper> g_KlineHash;              // 不同合约的k线存储表

// 交易参数
CThostFtdcTraderApi *g_pTradeUserApi = nullptr;                    // 交易指针
//180.168.146.187:10201
//180.168.146.187:10202
//218.202.237.33:10203
//180.168.146.187:10130
char gTradeFrontAddr[100];            // 模拟交易前置地址
TThostFtdcInstrumentIDType g_pTradeInstrumentID;        // 所交易的合约代码
TThostFtdcAuthCodeType gChAuthCode = "0000000000000000";
TThostFtdcAppIDType	gChAppID = "simnow_client_test";

std::unordered_map<std::string, std::shared_ptr<Strategy>> g_StrategyMap;
int gBarTimes;

void initStrategy(CustomTradeSpi *pTradeSpi) {
	std::string left, right, volume;
	getConfig("Strategy","LeftBars", left);
	getConfig("Strategy", "RightBars", right);
	getConfig("Strategy", "Volume", volume);
	auto pivotReversalStrategyPtr = std::make_shared<PivotReversalStrategy>();
	pivotReversalStrategyPtr->setLRBars(std::stoi(left), std::stoi(right));
	g_StrategyMap.emplace(g_pTradeInstrumentID, pivotReversalStrategyPtr);
	g_StrategyMap[g_pTradeInstrumentID]->setInstrument(g_pTradeInstrumentID, pTradeSpi);
	g_StrategyMap[g_pTradeInstrumentID]->setVolume(std::stoi(volume));
	g_StrategyMap[g_pTradeInstrumentID]->init();
}

void initConfig() {
	std::string brokerID, investerID, password, mdFrontAddr, instrumentID, tradeFrontAddr, authCode, appID, barTimes;
	getConfig("config", "BrokerID", brokerID);
	strcpy_s(gBrokerID, brokerID.c_str());
	getConfig("config", "InvesterID", investerID);
	strcpy_s(gInvesterID, investerID.c_str());
	getConfig("config", "Password", password);
	strcpy_s(gInvesterPassword, password.c_str());
	
	getConfig("config", "MdFrontAddr", mdFrontAddr);
	strcpy_s(gMdFrontAddr, mdFrontAddr.c_str());

	getConfig("config", "InstrumentID", instrumentID);
	strcpy_s(g_pTradeInstrumentID, instrumentID.c_str());

	getConfig("config", "TradeFrontAddr", tradeFrontAddr);
	strcpy_s(gTradeFrontAddr, tradeFrontAddr.c_str());

	getConfig("config", "AuthCode", authCode);
	strcpy_s(gChAuthCode, authCode.c_str());

	getConfig("config", "AppID", appID);
	strcpy_s(gChAppID, appID.c_str());

	getConfig("config", "BarTimes", barTimes);
	gBarTimes = std::stoi(barTimes);

	g_pInstrumentID[0] = g_pTradeInstrumentID;
}

int main()
{
	initConfig();

	// 初始化行情线程
	cout << "初始化行情..." << endl;
	g_pMdUserApi = CThostFtdcMdApi::CreateFtdcMdApi();   // 创建行情实例
	CThostFtdcMdSpi *pMdUserSpi = new CustomMdSpi;       // 创建行情回调实例
	g_pMdUserApi->RegisterSpi(pMdUserSpi);               // 注册事件类
	g_pMdUserApi->RegisterFront(gMdFrontAddr);           // 设置行情前置地址
	g_pMdUserApi->Init();                                // 连接运行
	


	// 初始化交易线程
	cout << "初始化交易..." << endl;
	g_pTradeUserApi = CThostFtdcTraderApi::CreateFtdcTraderApi(); // 创建交易实例
	//CThostFtdcTraderSpi *pTradeSpi = new CustomTradeSpi;
	CustomTradeSpi *pTradeSpi = new CustomTradeSpi;               // 创建交易回调实例
	g_pTradeUserApi->RegisterSpi(pTradeSpi);                      // 注册事件类
	g_pTradeUserApi->SubscribePublicTopic(THOST_TERT_RESUME);    // 订阅公共流
	g_pTradeUserApi->SubscribePrivateTopic(THOST_TERT_RESUME);   // 订阅私有流
	g_pTradeUserApi->RegisterFront(gTradeFrontAddr);              // 设置交易前置地址
	g_pTradeUserApi->Init();                                      // 连接运行
	
	initStrategy(pTradeSpi);

	// 等到线程退出
	g_pMdUserApi->Join();
	delete pMdUserSpi;
	g_pMdUserApi->Release();

	g_pTradeUserApi->Join();
	delete pTradeSpi;
	g_pTradeUserApi->Release();

	// 转换本地k线数据
	//TickToKlineHelper tickToKlineHelper;
	//tickToKlineHelper.KLineFromLocalData("market_data.csv", "K_line_data.csv");
	
	getchar();
	return 0;
}