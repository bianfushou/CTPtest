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
TThostFtdcBrokerIDType gBrokerID = "9999";                         // ģ�⾭���̴���
TThostFtdcInvestorIDType gInvesterID = "207346";                         // Ͷ�����˻���
TThostFtdcPasswordType gInvesterPassword = "SB@simnow12";                     // Ͷ��������

// �������
CThostFtdcMdApi *g_pMdUserApi = nullptr;                           // ����ָ��
//180.168.146.187:10211
//180.168.146.187:10212
//218.202.237.33:10213
//180.168.146.187:10131
char gMdFrontAddr[] = "tcp://180.168.146.187:10212";               // ģ������ǰ�õ�ַ
char *g_pInstrumentID[] = {"au2304"}; // �����Լ�����б��С��ϡ���֣��������ѡһ��
int instrumentNum = 1;                                             // �����Լ��������
unordered_map<string, TickToKlineHelper> g_KlineHash;              // ��ͬ��Լ��k�ߴ洢��

// ���ײ���
CThostFtdcTraderApi *g_pTradeUserApi = nullptr;                    // ����ָ��
//180.168.146.187:10201
//180.168.146.187:10202
//218.202.237.33:10203
//180.168.146.187:10130
char gTradeFrontAddr[] = "tcp://180.168.146.187:10202";            // ģ�⽻��ǰ�õ�ַ
TThostFtdcInstrumentIDType g_pTradeInstrumentID = "au2304";        // �����׵ĺ�Լ����
TThostFtdcDirectionType gTradeDirection = THOST_FTDC_D_Sell;       // ��������
TThostFtdcPriceType gLimitPrice = 22735;                           // ���׼۸�
TThostFtdcAuthCodeType gChAuthCode = "0000000000000000";
TThostFtdcAppIDType	gChAppID = "simnow_client_test";

std::unordered_map<std::string, std::shared_ptr<Strategy>> g_StrategyMap;

void initStrategy(CustomTradeSpi *pTradeSpi) {
	auto pivotReversalStrategyPtr = std::make_shared<PivotReversalStrategy>();
	pivotReversalStrategyPtr->setLRBars(3,2);
	g_StrategyMap.emplace(g_pTradeInstrumentID, pivotReversalStrategyPtr);
	g_StrategyMap[g_pTradeInstrumentID]->setInstrument(g_pTradeInstrumentID, pTradeSpi);
	g_StrategyMap[g_pTradeInstrumentID]->setVolume(1);
	g_StrategyMap[g_pTradeInstrumentID]->init();
}

int main()
{

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