#include <iostream>
#include <stdio.h>
#include <string>
#include <unordered_map>
#include "CustomMdSpi.h"
#include "CustomTradeSpi.h"
#include "TickToKlineHelper.h"
#include "StrategyTrade.h"

using namespace std;

// ���ӿ�
#pragma comment (lib, "thostmduserapi_se.lib")
#pragma comment (lib, "thosttraderapi_se.lib")

// ---- ȫ�ֱ��� ---- //
// ��������
TThostFtdcBrokerIDType gBrokerID;                         // ģ�⾭���̴���
TThostFtdcInvestorIDType gInvesterID;                         // Ͷ�����˻���
TThostFtdcPasswordType gInvesterPassword;                     // Ͷ��������

// �������
CThostFtdcMdApi *g_pMdUserApi = nullptr;                           // ����ָ��
//180.168.146.187:10211
//180.168.146.187:10212
//218.202.237.33:10213
//180.168.146.187:10131
char gMdFrontAddr[100];               // ģ������ǰ�õ�ַ
char *g_pInstrumentID[1]; // �����Լ�����б��С��ϡ���֣��������ѡһ��
int instrumentNum = 1;                                             // �����Լ��������
unordered_map<string, TickToKlineHelper> g_KlineHash;              // ��ͬ��Լ��k�ߴ洢��

// ���ײ���
CThostFtdcTraderApi *g_pTradeUserApi = nullptr;                    // ����ָ��
//180.168.146.187:10201
//180.168.146.187:10202
//218.202.237.33:10203
//180.168.146.187:10130
char gTradeFrontAddr[100];            // ģ�⽻��ǰ�õ�ַ
TThostFtdcInstrumentIDType g_pTradeInstrumentID;        // �����׵ĺ�Լ����
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

	// ��ʼ�������߳�
	cout << "��ʼ������..." << endl;
	g_pMdUserApi = CThostFtdcMdApi::CreateFtdcMdApi();   // ��������ʵ��
	CThostFtdcMdSpi *pMdUserSpi = new CustomMdSpi;       // ��������ص�ʵ��
	g_pMdUserApi->RegisterSpi(pMdUserSpi);               // ע���¼���
	g_pMdUserApi->RegisterFront(gMdFrontAddr);           // ��������ǰ�õ�ַ
	g_pMdUserApi->Init();                                // ��������
	


	// ��ʼ�������߳�
	cout << "��ʼ������..." << endl;
	g_pTradeUserApi = CThostFtdcTraderApi::CreateFtdcTraderApi(); // ��������ʵ��
	//CThostFtdcTraderSpi *pTradeSpi = new CustomTradeSpi;
	CustomTradeSpi *pTradeSpi = new CustomTradeSpi;               // �������׻ص�ʵ��
	g_pTradeUserApi->RegisterSpi(pTradeSpi);                      // ע���¼���
	g_pTradeUserApi->SubscribePublicTopic(THOST_TERT_RESUME);    // ���Ĺ�����
	g_pTradeUserApi->SubscribePrivateTopic(THOST_TERT_RESUME);   // ����˽����
	g_pTradeUserApi->RegisterFront(gTradeFrontAddr);              // ���ý���ǰ�õ�ַ
	g_pTradeUserApi->Init();                                      // ��������
	
	initStrategy(pTradeSpi);

	// �ȵ��߳��˳�
	g_pMdUserApi->Join();
	delete pMdUserSpi;
	g_pMdUserApi->Release();

	g_pTradeUserApi->Join();
	delete pTradeSpi;
	g_pTradeUserApi->Release();

	// ת������k������
	//TickToKlineHelper tickToKlineHelper;
	//tickToKlineHelper.KLineFromLocalData("market_data.csv", "K_line_data.csv");
	
	getchar();
	return 0;
}