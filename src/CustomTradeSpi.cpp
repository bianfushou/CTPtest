#include <iostream>
#include <time.h>
#include <thread>
#include <chrono>
#include <unordered_map>
#include "CustomTradeSpi.h"
#include "StrategyTrade.h"

// ---- ȫ�ֲ������� ---- //
extern TThostFtdcBrokerIDType gBrokerID;                      // ģ�⾭���̴���
extern TThostFtdcInvestorIDType gInvesterID;                  // Ͷ�����˻���
extern TThostFtdcPasswordType gInvesterPassword;              // Ͷ��������
extern CThostFtdcTraderApi *g_pTradeUserApi;                  // ����ָ��
extern char gTradeFrontAddr[];                                // ģ�⽻��ǰ�õ�ַ
extern TThostFtdcInstrumentIDType g_pTradeInstrumentID;       // �����׵ĺ�Լ����
extern TThostFtdcDirectionType gTradeDirection;               // ��������
extern TThostFtdcPriceType gLimitPrice;                       // ���׼۸�
extern TThostFtdcAuthCodeType gChAuthCode;                    //��֤��
extern TThostFtdcAppIDType	gChAppID;
extern std::unordered_map<std::string, std::shared_ptr<Strategy>> g_StrategyMap;
extern std::unordered_map<std::string, TickToKlineHelper> g_KlineHash;


// �Ự����
TThostFtdcFrontIDType	trade_front_id;	//ǰ�ñ��
TThostFtdcSessionIDType	session_id;	//�Ự���
TThostFtdcOrderRefType	order_ref;	//��������
time_t lOrderTime;
time_t lOrderOkTime;



void CustomTradeSpi::OnFrontConnected()
{
	if (!tradeLog) {
		std::string fileName = Logger::initFileName("CustomTrade");
		tradeLog = new Logger(fileName);
	}
	bool open = tradeLog->logIsOpen();
	tradeLog->logInfo( "=====�����������ӳɹ�=====");
	reqAuthenticate();
}

void CustomTradeSpi::OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField, CThostFtdcRspInfoField *pRspInfo,
	int nRequestID, bool bIsLast) {
	if (!isErrorRspInfo(pRspInfo))
	{
		tradeLog->logInfo("=====�˻���֤�ɹ�=====" );
		// ��ʼ��¼
		reqUserLogin();
	}
}

void CustomTradeSpi::OnRspUserLogin(
	CThostFtdcRspUserLoginField *pRspUserLogin,
	CThostFtdcRspInfoField *pRspInfo,
	int nRequestID,
	bool bIsLast)
{
	if (!isErrorRspInfo(pRspInfo))
	{
		tradeLog->logInfo("=====�˻���¼�ɹ�=====" );
		loginFlag = true;
		tradeLog->stringLog << "�����գ� " << pRspUserLogin->TradingDay;
		tradeLog->logInfo();
		tradeLog->stringLog << "��¼ʱ�䣺 " << pRspUserLogin->LoginTime;
		tradeLog->logInfo();
		tradeLog->stringLog << "�����̣� " << pRspUserLogin->BrokerID ;
		tradeLog->logInfo();
		tradeLog->stringLog << "�ʻ����� " << pRspUserLogin->UserID;
		tradeLog->logInfo();
		// ����Ự����
		trade_front_id = pRspUserLogin->FrontID;
		session_id = pRspUserLogin->SessionID;
		strcpy(order_ref, pRspUserLogin->MaxOrderRef);

		// Ͷ���߽�����ȷ��
		if (bIsLast) {
			reqSettlementInfoConfirm();
		}
	}
}

void CustomTradeSpi::OnRspError(CThostFtdcRspInfoField *pRspInfo, int nRequestID, bool bIsLast)
{
	isErrorRspInfo(pRspInfo);
}

void CustomTradeSpi::OnFrontDisconnected(int nReason)
{
	if (!tradeLog) {
		std::string fileName = Logger::initFileName("CustomTrade");
		tradeLog = new Logger(fileName);
	}
	bool open = tradeLog->logIsOpen();
	tradeLog->logErr( "=====�������ӶϿ�=====" );
	tradeLog->stringLog << "�����룺 " << nReason;
	tradeLog->logErr();
}

void CustomTradeSpi::OnHeartBeatWarning(int nTimeLapse)
{
	tradeLog->logErr("=====����������ʱ=====");
	tradeLog->stringLog << "���ϴ�����ʱ�䣺 " << nTimeLapse;
	tradeLog->logErr();
}

void CustomTradeSpi::OnRspUserLogout(
	CThostFtdcUserLogoutField *pUserLogout,
	CThostFtdcRspInfoField *pRspInfo,
	int nRequestID,
	bool bIsLast)
{
	if (!isErrorRspInfo(pRspInfo))
	{
		loginFlag = false; // �ǳ��Ͳ����ٽ����� 
		tradeLog->logInfo("=====�˻��ǳ��ɹ�=====" );
		tradeLog->stringLog << "�����̣� " << pUserLogout->BrokerID;
		tradeLog->logInfo();
		tradeLog->stringLog << "�ʻ����� " << pUserLogout->UserID;
		tradeLog->logInfo();
	}
}

void CustomTradeSpi::OnRspSettlementInfoConfirm(
	CThostFtdcSettlementInfoConfirmField *pSettlementInfoConfirm,
	CThostFtdcRspInfoField *pRspInfo,
	int nRequestID,
	bool bIsLast)
{
	if (!isErrorRspInfo(pRspInfo))
	{
		tradeLog->logInfo("=====Ͷ���߽�����ȷ�ϳɹ�=====" );
		tradeLog->stringLog << "ȷ�����ڣ� " << pSettlementInfoConfirm->ConfirmDate;
		tradeLog->logInfo();
		tradeLog->stringLog << "ȷ��ʱ�䣺 " << pSettlementInfoConfirm->ConfirmTime;
		tradeLog->logInfo();
		// �����ѯ��Լ
		if (bIsLast) {
			reqQueryInstrument();
		}
	}
}

void CustomTradeSpi::OnRspQryInstrument(
	CThostFtdcInstrumentField *pInstrument,
	CThostFtdcRspInfoField *pRspInfo,
	int nRequestID,
	bool bIsLast)
{
	if (!isErrorRspInfo(pRspInfo))
	{
		tradeLog->logInfo("=====��ѯ��Լ����ɹ�=====");
		tradeLog->stringLog << "���������룺 " << pInstrument->ExchangeID;
		tradeLog->logInfo();
		tradeLog->stringLog << "��Լ���룺 " << pInstrument->InstrumentID << std::endl;
		tradeLog->stringLog << "��Լ�ڽ������Ĵ��룺 " << pInstrument->ExchangeInstID << std::endl;
		tradeLog->stringLog << "ִ�мۣ� " << pInstrument->StrikePrice << std::endl;
		tradeLog->stringLog << "�����գ� " << pInstrument->EndDelivDate << std::endl;
		tradeLog->stringLog << "��ǰ����״̬�� " << pInstrument->IsTrading << std::endl;
		tradeLog->logInfo();
		// �����ѯͶ�����ʽ��˻�
		if (bIsLast) {
			InstrumentFieldMap.emplace(std::string(pInstrument->InstrumentID), *pInstrument);
			reqQueryTradingAccount();
		}
	}
}

void CustomTradeSpi::OnRspQryTradingAccount(
	CThostFtdcTradingAccountField *pTradingAccount,
	CThostFtdcRspInfoField *pRspInfo,
	int nRequestID,
	bool bIsLast)
{
	if (!isErrorRspInfo(pRspInfo))
	{
		tradeLog->logInfo("=====��ѯͶ�����ʽ��˻��ɹ�=====" );
		tradeLog->stringLog << "Ͷ�����˺ţ� " << pTradingAccount->AccountID << std::endl;
		tradeLog->stringLog << "�����ʽ� " << pTradingAccount->Available << std::endl;
		tradeLog->stringLog << "��ȡ�ʽ� " << pTradingAccount->WithdrawQuota << std::endl;
		tradeLog->stringLog << "��ǰ��֤��: " << pTradingAccount->CurrMargin << std::endl;
		tradeLog->stringLog << "ƽ��ӯ���� " << pTradingAccount->CloseProfit << std::endl;
		tradeLog->logInfo();
		// �����ѯͶ���ֲ߳�
		if (bIsLast) {
			reqQueryInvestorPosition();
		}
		
	}
}

void CustomTradeSpi::OnRspQryInvestorPosition(
	CThostFtdcInvestorPositionField *pInvestorPosition,
	CThostFtdcRspInfoField *pRspInfo,
	int nRequestID,
	bool bIsLast)
{
	if (!isErrorRspInfo(pRspInfo))
	{
		tradeLog->logInfo("=====��ѯͶ���ֲֳ߳ɹ�=====" );
		if (pInvestorPosition)
		{
			tradeLog->stringLog << "��Լ���룺 " << pInvestorPosition->InstrumentID << std::endl;
			tradeLog->stringLog << "���ּ۸� " << pInvestorPosition->OpenAmount << std::endl;
			tradeLog->stringLog << "�������� " << pInvestorPosition->OpenVolume << std::endl;
			tradeLog->stringLog << "���ַ��� " << pInvestorPosition->PosiDirection << std::endl;
			tradeLog->stringLog << "ռ�ñ�֤��" << pInvestorPosition->UseMargin << std::endl;
			tradeLog->logInfo();
		}
		else
			tradeLog->logInfo("----->�ú�Լδ�ֲ�" );
		
		// ����¼������������һ���ӿڣ��˴��ǰ�˳��ִ�У�
		/*if (loginFlag)
			reqOrderInsert();*/
		//if (loginFlag)
		//	reqOrderInsertWithParams(g_pTradeInstrumentID, gLimitPrice, 1, gTradeDirection); // �Զ���һ�ʽ���

		// ���Խ���
		tradeLog->logInfo("=====��ʼ������Խ���=====" );
		std::string tradeInstrumentID(g_pTradeInstrumentID);
		//reqOrderInsert();
		
		tradeStrategyTasks.emplace_back([this, tradeInstrumentID]() {
				while (loginFlag && !taskStop) {
					std::this_thread::sleep_for(std::chrono::milliseconds(100));
					g_StrategyMap[tradeInstrumentID]->operator()();
				}
			}
		);
		
	}
}

void CustomTradeSpi::OnRspOrderInsert(
	CThostFtdcInputOrderField *pInputOrder, 
	CThostFtdcRspInfoField *pRspInfo,
	int nRequestID,
	bool bIsLast)
{
	if (!isErrorRspInfo(pRspInfo))
	{
		tradeLog->logInfo("=====����¼��ɹ�=====");
		tradeLog->stringLog << "��Լ���룺 " << pInputOrder->InstrumentID << std::endl;
		tradeLog->stringLog << "�۸� " << pInputOrder->LimitPrice << std::endl;
		tradeLog->stringLog << "������ " << pInputOrder->VolumeTotalOriginal << std::endl;
		tradeLog->stringLog << "���ַ��� " << pInputOrder->Direction << std::endl;
		tradeLog->logInfo();
	}
	else {
		PivotReversalStrategy* strategy = dynamic_cast<PivotReversalStrategy*>(g_StrategyMap[std::string(pInputOrder->InstrumentID)].get());
		if (strategy) {
			strategy->resetStatus();
		}
	}
}

void CustomTradeSpi::OnRspOrderAction(
	CThostFtdcInputOrderActionField *pInputOrderAction,
	CThostFtdcRspInfoField *pRspInfo,
	int nRequestID,
	bool bIsLast)
{
	if (!isErrorRspInfo(pRspInfo))
	{
		tradeLog->logInfo("=====���������ɹ�=====" );
		tradeLog->stringLog << "��Լ���룺 " << pInputOrderAction->InstrumentID << std::endl;
		tradeLog->stringLog << "������־�� " << pInputOrderAction->ActionFlag;
		tradeLog->logInfo();
	}
}

void CustomTradeSpi::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
	char str[10];
	sprintf(str, "%d", pOrder->OrderSubmitStatus);
	int orderState = atoi(str) - 48;	//����״̬0=�Ѿ��ύ��3=�Ѿ�����

	tradeLog->logInfo("=====�յ�����Ӧ��=====" );

	if (isMyOrder(pOrder))
	{
		if (isTradingOrder(pOrder))
		{
			tradeLog->logInfo("--->>> �ȴ��ɽ��У�");
			//reqOrderAction(pOrder); // ������Գ���
			//reqUserLogout(); // �ǳ�����
		}
		else if (pOrder->OrderStatus == THOST_FTDC_OST_Canceled)
		{
			
			PivotReversalStrategy* strategy = dynamic_cast<PivotReversalStrategy*>(g_StrategyMap[std::string(pOrder->InstrumentID)].get());
			if (strategy) {
				strategy->resetStatus();
				tradeLog->logInfo("--->>> �����ɹ���");
			}
			
		}
			
	}
}

void CustomTradeSpi::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
	tradeLog->logInfo("=====�����ɹ��ɽ�=====" );
	tradeLog->stringLog << "�ɽ�ʱ�䣺 " << pTrade->TradeTime << std::endl;
	tradeLog->stringLog << "��Լ���룺 " << pTrade->InstrumentID << std::endl;
	tradeLog->stringLog << "�ɽ��۸� " << pTrade->Price << std::endl;
	tradeLog->stringLog << "�ɽ����� " << pTrade->Volume << std::endl;
	tradeLog->stringLog << "��ƽ�ַ��� " << pTrade->Direction << std::endl;
	tradeLog->stringLog << pTrade->OffsetFlag << std::endl;
	tradeLog->logInfo();
	PivotReversalStrategy* strategy = dynamic_cast<PivotReversalStrategy*>(g_StrategyMap[std::string(pTrade->InstrumentID)].get());
	if (strategy) {
		strategy->statusDone();
		tradeLog->logInfo("******Trade success******");
	}
}

bool CustomTradeSpi::isErrorRspInfo(CThostFtdcRspInfoField *pRspInfo)
{
	bool bResult = pRspInfo && (pRspInfo->ErrorID != 0);
	if (bResult) {
		tradeLog->stringLog << "���ش���--->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << pRspInfo->ErrorMsg;
		tradeLog->logErr();
		if (pRspInfo->ErrorID == 90) {
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
			curReqFun();
		}
	}
	return bResult;
}

void CustomTradeSpi::reqAuthenticate() {
	CThostFtdcReqAuthenticateField auth = { 0 };
	strcpy_s(auth.BrokerID, gBrokerID);
	strcpy_s(auth.UserID, gInvesterID);
	//strcpy_s(a.UserProductInfo, "");
	strcpy_s(auth.AuthCode, gChAuthCode);
	strcpy_s(auth.AppID, gChAppID);
	static int requestID = 0; // ������
	int rt = g_pTradeUserApi->ReqAuthenticate(&auth, requestID);
	if (!rt) {
		tradeLog->logInfo(">>>>>>������֤����ɹ�");
		curReqFun = std::bind(&CustomTradeSpi::reqAuthenticate, this);
	}
	else
		tradeLog->logErr("--->>>������֤����ʧ��" );
}

void CustomTradeSpi::reqUserLogin()
{
	CThostFtdcReqUserLoginField loginReq;
	memset(&loginReq, 0, sizeof(loginReq));
	strcpy(loginReq.BrokerID, gBrokerID);
	strcpy(loginReq.UserID, gInvesterID);
	strcpy(loginReq.Password, gInvesterPassword);
	static int requestID = 0; // ������
	int rt = g_pTradeUserApi->ReqUserLogin(&loginReq, requestID);
	if (!rt) {
		tradeLog->logInfo(">>>>>>���͵�¼����ɹ�");
		curReqFun = std::bind(&CustomTradeSpi::reqUserLogin, this);
	}
	else
		tradeLog->logErr("--->>>���͵�¼����ʧ��");
}

void CustomTradeSpi::reqUserLogout()
{
	CThostFtdcUserLogoutField logoutReq;
	memset(&logoutReq, 0, sizeof(logoutReq));
	strcpy(logoutReq.BrokerID, gBrokerID);
	strcpy(logoutReq.UserID, gInvesterID);
	static int requestID = 0; // ������
	int rt = g_pTradeUserApi->ReqUserLogout(&logoutReq, requestID);
	if (!rt) {
		tradeLog->logInfo(">>>>>>���͵ǳ�����ɹ�");
		curReqFun = std::bind(&CustomTradeSpi::reqUserLogout, this);
	}
	else
		tradeLog->logErr("--->>>���͵ǳ�����ʧ��" );
}


void CustomTradeSpi::reqSettlementInfoConfirm()
{
	CThostFtdcSettlementInfoConfirmField settlementConfirmReq;
	memset(&settlementConfirmReq, 0, sizeof(settlementConfirmReq));
	strcpy(settlementConfirmReq.BrokerID, gBrokerID);
	strcpy(settlementConfirmReq.InvestorID, gInvesterID);
	static int requestID = 0; // ������
	int rt = g_pTradeUserApi->ReqSettlementInfoConfirm(&settlementConfirmReq, requestID);
	if (!rt) {
		tradeLog->logInfo(">>>>>>����Ͷ���߽�����ȷ������ɹ�");
		curReqFun = std::bind(&CustomTradeSpi::reqSettlementInfoConfirm, this);
	}
	else if (rt = -2 || rt == -3) {

		tradeLog->logErr("--->>>����Ͷ���߽�����ȷ������ʧ��");
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		reqSettlementInfoConfirm();
	}
	else {
		tradeLog->logErr("--->>>����Ͷ���߽�����ȷ������ʧ��");
		tradeLog->logErr("--->>>��������ʧ��");
	}	
}

void CustomTradeSpi::reqQueryInstrument()
{
	CThostFtdcQryInstrumentField instrumentReq;
	memset(&instrumentReq, 0, sizeof(instrumentReq));
	strcpy(instrumentReq.InstrumentID, g_pTradeInstrumentID);
	static int requestID = 0; // ������
	int rt = g_pTradeUserApi->ReqQryInstrument(&instrumentReq, requestID);
	if (!rt) {
		tradeLog->logInfo(">>>>>>���ͺ�Լ��ѯ����ɹ�");
		curReqFun = std::bind(&CustomTradeSpi::reqQueryInstrument, this);
	}
	else if(rt = -2 || rt == -3){
		
		tradeLog->logErr("--->>>���ͺ�Լ��ѯ����ʧ��");
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		reqQueryInstrument();
	}
	else {
		tradeLog->logErr("--->>>���ͺ�Լ��ѯ����ʧ��");
		tradeLog->logErr("--->>>��������ʧ��");
	}	
}

void CustomTradeSpi::reqQueryTradingAccount()
{
	CThostFtdcQryTradingAccountField tradingAccountReq;
	memset(&tradingAccountReq, 0, sizeof(tradingAccountReq));
	strcpy(tradingAccountReq.BrokerID, gBrokerID);
	strcpy(tradingAccountReq.InvestorID, gInvesterID);
	strcpy_s(tradingAccountReq.CurrencyID, "CNY");
	static int requestID = 0; // ������
	std::this_thread::sleep_for(std::chrono::milliseconds(500)); // ��ʱ����Ҫͣ��һ����ܲ�ѯ�ɹ�
	int rt = g_pTradeUserApi->ReqQryTradingAccount(&tradingAccountReq, requestID);
	if (!rt) {
		tradeLog->logInfo(">>>>>>����Ͷ�����ʽ��˻���ѯ����ɹ�");
		curReqFun = std::bind(&CustomTradeSpi::reqQueryTradingAccount, this);
	}
	else if (rt = -2 || rt == -3) {

		tradeLog->logErr("--->>>����Ͷ���߽�����ȷ������ʧ��");
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		reqQueryTradingAccount();
	}
	else {
		tradeLog->logErr("--->>>����Ͷ�����ʽ��˻���ѯ����ʧ��");
		tradeLog->logErr("--->>>��������ʧ��");
	}
}

void CustomTradeSpi::reqQueryInvestorPosition()
{
	CThostFtdcQryInvestorPositionField postionReq;
	memset(&postionReq, 0, sizeof(postionReq));
	strcpy(postionReq.BrokerID, gBrokerID);
	strcpy(postionReq.InvestorID, gInvesterID);
	strcpy(postionReq.InstrumentID, g_pTradeInstrumentID);
	static int requestID = 0; // ������
	std::this_thread::sleep_for(std::chrono::milliseconds(500)); // ��ʱ����Ҫͣ��һ����ܲ�ѯ�ɹ�
	int rt = g_pTradeUserApi->ReqQryInvestorPosition(&postionReq, requestID);
	if (!rt) {
		tradeLog->logInfo(">>>>>>����Ͷ���ֲֲ߳�ѯ����ɹ�");
		curReqFun = std::bind(&CustomTradeSpi::reqQueryInvestorPosition, this);
	}
	else if (rt = -2 || rt == -3) {

		tradeLog->logErr("--->>>����Ͷ���ֲֲ߳�ѯ����ʧ��");
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		reqQueryInvestorPosition();
	}
	else {
		tradeLog->logErr("--->>>����Ͷ���ֲֲ߳�ѯ����ʧ��");
		tradeLog->logErr("--->>>��������ʧ��");
	}	
}

void CustomTradeSpi::reqOrderInsert()
{
	CThostFtdcInputOrderField orderInsertReq;
	memset(&orderInsertReq, 0, sizeof(CThostFtdcInputOrderField));
	///���͹�˾����
	strcpy(orderInsertReq.BrokerID, gBrokerID);
	///Ͷ���ߴ���
	strcpy(orderInsertReq.InvestorID, gInvesterID);
	///��Լ����
	strcpy(orderInsertReq.InstrumentID, g_pTradeInstrumentID);
	///��������
	//strcpy(orderInsertReq.OrderRef, order_ref);
	///�����۸�����: �޼�
	orderInsertReq.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
	///��������: 
	orderInsertReq.Direction = THOST_FTDC_D_Buy;
	///��Ͽ�ƽ��־: ����
	orderInsertReq.CombOffsetFlag[0] = THOST_FTDC_OF_Open;
	///���Ͷ���ױ���־
	orderInsertReq.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
	///�۸�
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	TickToKlineHelper& tickToKlineObject = g_KlineHash.at(std::string(g_pTradeInstrumentID));
	orderInsertReq.LimitPrice = tickToKlineObject.lastPrice;
	///������1
	orderInsertReq.VolumeTotalOriginal = 1;
	///��Ч������: ������Ч
	orderInsertReq.TimeCondition = THOST_FTDC_TC_GFD;
	///�ɽ�������: �κ�����
	orderInsertReq.VolumeCondition = THOST_FTDC_VC_AV;
	///��С�ɽ���: 1
	orderInsertReq.MinVolume = 1;
	///��������: ����
	orderInsertReq.ContingentCondition = THOST_FTDC_CC_Immediately;
	orderInsertReq.StopPrice = 0;
	///ǿƽԭ��: ��ǿƽ
	orderInsertReq.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
	///�Զ������־: ��
	orderInsertReq.IsAutoSuspend = 0;
	///�û�ǿ����־: ��
	orderInsertReq.UserForceClose = 0;
	strcpy_s(orderInsertReq.ExchangeID, InstrumentFieldMap[orderInsertReq.InstrumentID].ExchangeID);

	static int requestID = 0; // ������
	int rt = g_pTradeUserApi->ReqOrderInsert(&orderInsertReq, ++requestID);
	if (!rt) {
		tradeLog->logInfo(">>>>>>���ͱ���¼������ɹ�");
		curReqFun = [this]() {this->reqOrderInsert(); };
	}
	else
		tradeLog->logErr("--->>>���ͱ���¼������ʧ��");
}

void CustomTradeSpi::reqOrderInsert(
	const TThostFtdcInstrumentIDType instrumentID,
	TThostFtdcPriceType price,
	TThostFtdcVolumeType volume,
	TThostFtdcDirectionType direction)
{
	CThostFtdcInputOrderField orderInsertReq;
	memset(&orderInsertReq, 0, sizeof(orderInsertReq));
	///���͹�˾����
	strcpy(orderInsertReq.BrokerID, gBrokerID);
	///Ͷ���ߴ���
	strcpy(orderInsertReq.InvestorID, gInvesterID);
	///��Լ����
	strcpy(orderInsertReq.InstrumentID, instrumentID);
	///��������
	strcpy(orderInsertReq.OrderRef, order_ref);
	///�����۸�����: �޼�
	orderInsertReq.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
	///��������: 
	orderInsertReq.Direction = direction;
	///��Ͽ�ƽ��־: ����
	orderInsertReq.CombOffsetFlag[0] = THOST_FTDC_OF_Open;
	///���Ͷ���ױ���־
	orderInsertReq.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
	///�۸�
	orderInsertReq.LimitPrice = price;
	///������1
	orderInsertReq.VolumeTotalOriginal = volume;
	///��Ч������: ������Ч
	orderInsertReq.TimeCondition = THOST_FTDC_TC_GFD;
	///�ɽ�������: �κ�����
	orderInsertReq.VolumeCondition = THOST_FTDC_VC_AV;
	///��С�ɽ���: 1
	orderInsertReq.MinVolume = 1;
	///��������: ����
	orderInsertReq.ContingentCondition = THOST_FTDC_CC_Immediately;
	///ǿƽԭ��: ��ǿƽ
	orderInsertReq.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
	///�Զ������־: ��
	orderInsertReq.IsAutoSuspend = 0;
	///�û�ǿ����־: ��
	orderInsertReq.UserForceClose = 0;

	static int requestID = 0; // ������
	int rt = g_pTradeUserApi->ReqOrderInsert(&orderInsertReq, ++requestID);
	if (!rt) {
		tradeLog->logInfo(">>>>>>���ͱ���¼������ɹ�");
		std::string  instrumentStr(instrumentID);
		curReqFun = [this, instrumentStr, price, volume, direction]() {this->reqOrderInsert(instrumentStr.c_str(), price, volume, direction); };
	}
	else
		tradeLog->logErr("--->>>���ͱ���¼������ʧ��");
}

void CustomTradeSpi::reqOrder(std::shared_ptr<CThostFtdcInputOrderField> orderInsertReqPtr, bool isDefault) {
	CThostFtdcInputOrderField& orderInsertReq = *orderInsertReqPtr;
	if (isDefault) {
		strcpy(orderInsertReq.BrokerID, gBrokerID);
		///Ͷ���ߴ���
		strcpy(orderInsertReq.InvestorID, gInvesterID);
		strcpy(orderInsertReq.UserID, gInvesterID);
		///��������
		//strcpy(orderInsertReq.OrderRef, order_ref);
		///�����۸�����: �޼�
		orderInsertReq.OrderPriceType = THOST_FTDC_OPT_LimitPrice; //THOST_FTDC_OPT_AnyPrice;
		///���Ͷ���ױ���־
		orderInsertReq.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
		///��Ч������: ������Ч
		orderInsertReq.TimeCondition = THOST_FTDC_TC_GFD;
		///�ɽ�������: �κ�����
		orderInsertReq.VolumeCondition = THOST_FTDC_VC_AV;

		strcpy_s(orderInsertReq.ExchangeID, InstrumentFieldMap[orderInsertReq.InstrumentID].ExchangeID);
		///��С�ɽ���: 1
		orderInsertReq.MinVolume = 1;
		///��������: ����
		orderInsertReq.ContingentCondition = THOST_FTDC_CC_Immediately;
		///ǿƽԭ��: ��ǿƽ
		orderInsertReq.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
		///�Զ������־: ��
		orderInsertReq.IsAutoSuspend = 0;
		///�û�ǿ����־: ��
		//orderInsertReq.UserForceClose = 0;

	}
	static int requestID = 0; // ������
	int rt = g_pTradeUserApi->ReqOrderInsert(orderInsertReqPtr.get(), ++requestID);
	if (!rt) {
		tradeLog->logInfo(">>>>>>���ͱ���¼������ɹ�");
		curReqFun = [this, orderInsertReqPtr, isDefault]() {this->reqOrder(orderInsertReqPtr, isDefault); };
	}
	else if (rt == -2 || rt == -3) {
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		reqOrder(orderInsertReqPtr, false);
	}
	else
		tradeLog->logErr("--->>>���ͱ���¼������ʧ��" );
}

void CustomTradeSpi::reqOrderAction(CThostFtdcOrderField *pOrder)
{
	static bool orderActionSentFlag = false; // �Ƿ����˱���
	if (orderActionSentFlag)
		return;

	CThostFtdcInputOrderActionField orderActionReq;
	memset(&orderActionReq, 0, sizeof(orderActionReq));
	///���͹�˾����
	strcpy(orderActionReq.BrokerID, pOrder->BrokerID);
	///Ͷ���ߴ���
	strcpy(orderActionReq.InvestorID, pOrder->InvestorID);
	///������������
	//	TThostFtdcOrderActionRefType	OrderActionRef;
	///��������
	strcpy(orderActionReq.OrderRef, pOrder->OrderRef);
	///������
	//	TThostFtdcRequestIDType	RequestID;
	///ǰ�ñ��
	orderActionReq.FrontID = trade_front_id;
	///�Ự���
	orderActionReq.SessionID = session_id;
	///����������
	//	TThostFtdcExchangeIDType	ExchangeID;
	///�������
	//	TThostFtdcOrderSysIDType	OrderSysID;
	///������־
	orderActionReq.ActionFlag = THOST_FTDC_AF_Delete;
	///�۸�
	//	TThostFtdcPriceType	LimitPrice;
	///�����仯
	//	TThostFtdcVolumeType	VolumeChange;
	///�û�����
	//	TThostFtdcUserIDType	UserID;
	///��Լ����
	strcpy(orderActionReq.InstrumentID, pOrder->InstrumentID);
	static int requestID = 0; // ������
	int rt = g_pTradeUserApi->ReqOrderAction(&orderActionReq, ++requestID);
	if (!rt)
		tradeLog->logInfo(">>>>>>���ͱ�����������ɹ�" );
	else
		tradeLog->logErr("--->>>���ͱ�����������ʧ��" );
	orderActionSentFlag = true;
}

bool CustomTradeSpi::isMyOrder(CThostFtdcOrderField *pOrder)
{
	return ((pOrder->FrontID == trade_front_id) &&
		(pOrder->SessionID == session_id));
}

bool CustomTradeSpi::isTradingOrder(CThostFtdcOrderField *pOrder)
{
	return ((pOrder->OrderStatus != THOST_FTDC_OST_PartTradedNotQueueing) &&
		(pOrder->OrderStatus != THOST_FTDC_OST_Canceled) &&
		(pOrder->OrderStatus != THOST_FTDC_OST_AllTraded));
}