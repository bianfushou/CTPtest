#include <iostream>
#include <time.h>
#include <thread>
#include <chrono>
#include <unordered_map>
#include "CustomTradeSpi.h"
#include "StrategyTrade.h"

// ---- 全局参数声明 ---- //
extern TThostFtdcBrokerIDType gBrokerID;                      // 模拟经纪商代码
extern TThostFtdcInvestorIDType gInvesterID;                  // 投资者账户名
extern TThostFtdcPasswordType gInvesterPassword;              // 投资者密码
extern CThostFtdcTraderApi *g_pTradeUserApi;                  // 交易指针
extern char gTradeFrontAddr[];                                // 模拟交易前置地址
extern TThostFtdcInstrumentIDType g_pTradeInstrumentID;       // 所交易的合约代码
extern TThostFtdcDirectionType gTradeDirection;               // 买卖方向
extern TThostFtdcPriceType gLimitPrice;                       // 交易价格
extern TThostFtdcAuthCodeType gChAuthCode;                    //认证码
extern TThostFtdcAppIDType	gChAppID;
extern std::unordered_map<std::string, std::shared_ptr<Strategy>> g_StrategyMap;
extern std::unordered_map<std::string, TickToKlineHelper> g_KlineHash;


// 会话参数
TThostFtdcFrontIDType	trade_front_id;	//前置编号
TThostFtdcSessionIDType	session_id;	//会话编号
TThostFtdcOrderRefType	order_ref;	//报单引用
time_t lOrderTime;
time_t lOrderOkTime;



void CustomTradeSpi::OnFrontConnected()
{
	if (!tradeLog) {
		std::string fileName = Logger::initFileName("CustomTrade");
		tradeLog = new Logger(fileName);
	}
	bool open = tradeLog->logIsOpen();
	tradeLog->logInfo( "=====建立网络连接成功=====");
	reqAuthenticate();
}

void CustomTradeSpi::OnRspAuthenticate(CThostFtdcRspAuthenticateField *pRspAuthenticateField, CThostFtdcRspInfoField *pRspInfo,
	int nRequestID, bool bIsLast) {
	if (!isErrorRspInfo(pRspInfo))
	{
		tradeLog->logInfo("=====账户认证成功=====" );
		// 开始登录
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
		tradeLog->logInfo("=====账户登录成功=====" );
		loginFlag = true;
		tradeLog->stringLog << "交易日： " << pRspUserLogin->TradingDay;
		tradeLog->logInfo();
		tradeLog->stringLog << "登录时间： " << pRspUserLogin->LoginTime;
		tradeLog->logInfo();
		tradeLog->stringLog << "经纪商： " << pRspUserLogin->BrokerID ;
		tradeLog->logInfo();
		tradeLog->stringLog << "帐户名： " << pRspUserLogin->UserID;
		tradeLog->logInfo();
		// 保存会话参数
		trade_front_id = pRspUserLogin->FrontID;
		session_id = pRspUserLogin->SessionID;
		strcpy(order_ref, pRspUserLogin->MaxOrderRef);

		// 投资者结算结果确认
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
	tradeLog->logErr( "=====网络连接断开=====" );
	tradeLog->stringLog << "错误码： " << nReason;
	tradeLog->logErr();
}

void CustomTradeSpi::OnHeartBeatWarning(int nTimeLapse)
{
	tradeLog->logErr("=====网络心跳超时=====");
	tradeLog->stringLog << "距上次连接时间： " << nTimeLapse;
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
		loginFlag = false; // 登出就不能再交易了 
		tradeLog->logInfo("=====账户登出成功=====" );
		tradeLog->stringLog << "经纪商： " << pUserLogout->BrokerID;
		tradeLog->logInfo();
		tradeLog->stringLog << "帐户名： " << pUserLogout->UserID;
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
		tradeLog->logInfo("=====投资者结算结果确认成功=====" );
		tradeLog->stringLog << "确认日期： " << pSettlementInfoConfirm->ConfirmDate;
		tradeLog->logInfo();
		tradeLog->stringLog << "确认时间： " << pSettlementInfoConfirm->ConfirmTime;
		tradeLog->logInfo();
		// 请求查询合约
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
		tradeLog->logInfo("=====查询合约结果成功=====");
		tradeLog->stringLog << "交易所代码： " << pInstrument->ExchangeID;
		tradeLog->logInfo();
		tradeLog->stringLog << "合约代码： " << pInstrument->InstrumentID << std::endl;
		tradeLog->stringLog << "合约在交易所的代码： " << pInstrument->ExchangeInstID << std::endl;
		tradeLog->stringLog << "执行价： " << pInstrument->StrikePrice << std::endl;
		tradeLog->stringLog << "到期日： " << pInstrument->EndDelivDate << std::endl;
		tradeLog->stringLog << "当前交易状态： " << pInstrument->IsTrading << std::endl;
		tradeLog->logInfo();
		// 请求查询投资者资金账户
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
		tradeLog->logInfo("=====查询投资者资金账户成功=====" );
		tradeLog->stringLog << "投资者账号： " << pTradingAccount->AccountID << std::endl;
		tradeLog->stringLog << "可用资金： " << pTradingAccount->Available << std::endl;
		tradeLog->stringLog << "可取资金： " << pTradingAccount->WithdrawQuota << std::endl;
		tradeLog->stringLog << "当前保证金: " << pTradingAccount->CurrMargin << std::endl;
		tradeLog->stringLog << "平仓盈亏： " << pTradingAccount->CloseProfit << std::endl;
		tradeLog->logInfo();
		// 请求查询投资者持仓
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
		tradeLog->logInfo("=====查询投资者持仓成功=====" );
		if (pInvestorPosition)
		{
			tradeLog->stringLog << "合约代码： " << pInvestorPosition->InstrumentID << std::endl;
			tradeLog->stringLog << "开仓价格： " << pInvestorPosition->OpenAmount << std::endl;
			tradeLog->stringLog << "开仓量： " << pInvestorPosition->OpenVolume << std::endl;
			tradeLog->stringLog << "开仓方向： " << pInvestorPosition->PosiDirection << std::endl;
			tradeLog->stringLog << "占用保证金：" << pInvestorPosition->UseMargin << std::endl;
			tradeLog->logInfo();
		}
		else
			tradeLog->logInfo("----->该合约未持仓" );
		
		// 报单录入请求（这里是一部接口，此处是按顺序执行）
		/*if (loginFlag)
			reqOrderInsert();*/
		//if (loginFlag)
		//	reqOrderInsertWithParams(g_pTradeInstrumentID, gLimitPrice, 1, gTradeDirection); // 自定义一笔交易

		// 策略交易
		tradeLog->logInfo("=====开始进入策略交易=====" );
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
		tradeLog->logInfo("=====报单录入成功=====");
		tradeLog->stringLog << "合约代码： " << pInputOrder->InstrumentID << std::endl;
		tradeLog->stringLog << "价格： " << pInputOrder->LimitPrice << std::endl;
		tradeLog->stringLog << "数量： " << pInputOrder->VolumeTotalOriginal << std::endl;
		tradeLog->stringLog << "开仓方向： " << pInputOrder->Direction << std::endl;
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
		tradeLog->logInfo("=====报单操作成功=====" );
		tradeLog->stringLog << "合约代码： " << pInputOrderAction->InstrumentID << std::endl;
		tradeLog->stringLog << "操作标志： " << pInputOrderAction->ActionFlag;
		tradeLog->logInfo();
	}
}

void CustomTradeSpi::OnRtnOrder(CThostFtdcOrderField *pOrder)
{
	char str[10];
	sprintf(str, "%d", pOrder->OrderSubmitStatus);
	int orderState = atoi(str) - 48;	//报单状态0=已经提交，3=已经接受

	tradeLog->logInfo("=====收到报单应答=====" );

	if (isMyOrder(pOrder))
	{
		if (isTradingOrder(pOrder))
		{
			tradeLog->logInfo("--->>> 等待成交中！");
			//reqOrderAction(pOrder); // 这里可以撤单
			//reqUserLogout(); // 登出测试
		}
		else if (pOrder->OrderStatus == THOST_FTDC_OST_Canceled)
		{
			
			PivotReversalStrategy* strategy = dynamic_cast<PivotReversalStrategy*>(g_StrategyMap[std::string(pOrder->InstrumentID)].get());
			if (strategy) {
				strategy->resetStatus();
				tradeLog->logInfo("--->>> 撤单成功！");
			}
			
		}
			
	}
}

void CustomTradeSpi::OnRtnTrade(CThostFtdcTradeField *pTrade)
{
	tradeLog->logInfo("=====报单成功成交=====" );
	tradeLog->stringLog << "成交时间： " << pTrade->TradeTime << std::endl;
	tradeLog->stringLog << "合约代码： " << pTrade->InstrumentID << std::endl;
	tradeLog->stringLog << "成交价格： " << pTrade->Price << std::endl;
	tradeLog->stringLog << "成交量： " << pTrade->Volume << std::endl;
	tradeLog->stringLog << "开平仓方向： " << pTrade->Direction << std::endl;
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
		tradeLog->stringLog << "返回错误--->>> ErrorID=" << pRspInfo->ErrorID << ", ErrorMsg=" << pRspInfo->ErrorMsg;
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
	static int requestID = 0; // 请求编号
	int rt = g_pTradeUserApi->ReqAuthenticate(&auth, requestID);
	if (!rt) {
		tradeLog->logInfo(">>>>>>发送认证请求成功");
		curReqFun = std::bind(&CustomTradeSpi::reqAuthenticate, this);
	}
	else
		tradeLog->logErr("--->>>发送认证请求失败" );
}

void CustomTradeSpi::reqUserLogin()
{
	CThostFtdcReqUserLoginField loginReq;
	memset(&loginReq, 0, sizeof(loginReq));
	strcpy(loginReq.BrokerID, gBrokerID);
	strcpy(loginReq.UserID, gInvesterID);
	strcpy(loginReq.Password, gInvesterPassword);
	static int requestID = 0; // 请求编号
	int rt = g_pTradeUserApi->ReqUserLogin(&loginReq, requestID);
	if (!rt) {
		tradeLog->logInfo(">>>>>>发送登录请求成功");
		curReqFun = std::bind(&CustomTradeSpi::reqUserLogin, this);
	}
	else
		tradeLog->logErr("--->>>发送登录请求失败");
}

void CustomTradeSpi::reqUserLogout()
{
	CThostFtdcUserLogoutField logoutReq;
	memset(&logoutReq, 0, sizeof(logoutReq));
	strcpy(logoutReq.BrokerID, gBrokerID);
	strcpy(logoutReq.UserID, gInvesterID);
	static int requestID = 0; // 请求编号
	int rt = g_pTradeUserApi->ReqUserLogout(&logoutReq, requestID);
	if (!rt) {
		tradeLog->logInfo(">>>>>>发送登出请求成功");
		curReqFun = std::bind(&CustomTradeSpi::reqUserLogout, this);
	}
	else
		tradeLog->logErr("--->>>发送登出请求失败" );
}


void CustomTradeSpi::reqSettlementInfoConfirm()
{
	CThostFtdcSettlementInfoConfirmField settlementConfirmReq;
	memset(&settlementConfirmReq, 0, sizeof(settlementConfirmReq));
	strcpy(settlementConfirmReq.BrokerID, gBrokerID);
	strcpy(settlementConfirmReq.InvestorID, gInvesterID);
	static int requestID = 0; // 请求编号
	int rt = g_pTradeUserApi->ReqSettlementInfoConfirm(&settlementConfirmReq, requestID);
	if (!rt) {
		tradeLog->logInfo(">>>>>>发送投资者结算结果确认请求成功");
		curReqFun = std::bind(&CustomTradeSpi::reqSettlementInfoConfirm, this);
	}
	else if (rt = -2 || rt == -3) {

		tradeLog->logErr("--->>>发送投资者结算结果确认请求失败");
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		reqSettlementInfoConfirm();
	}
	else {
		tradeLog->logErr("--->>>发送投资者结算结果确认请求失败");
		tradeLog->logErr("--->>>网络连接失败");
	}	
}

void CustomTradeSpi::reqQueryInstrument()
{
	CThostFtdcQryInstrumentField instrumentReq;
	memset(&instrumentReq, 0, sizeof(instrumentReq));
	strcpy(instrumentReq.InstrumentID, g_pTradeInstrumentID);
	static int requestID = 0; // 请求编号
	int rt = g_pTradeUserApi->ReqQryInstrument(&instrumentReq, requestID);
	if (!rt) {
		tradeLog->logInfo(">>>>>>发送合约查询请求成功");
		curReqFun = std::bind(&CustomTradeSpi::reqQueryInstrument, this);
	}
	else if(rt = -2 || rt == -3){
		
		tradeLog->logErr("--->>>发送合约查询请求失败");
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		reqQueryInstrument();
	}
	else {
		tradeLog->logErr("--->>>发送合约查询请求失败");
		tradeLog->logErr("--->>>网络连接失败");
	}	
}

void CustomTradeSpi::reqQueryTradingAccount()
{
	CThostFtdcQryTradingAccountField tradingAccountReq;
	memset(&tradingAccountReq, 0, sizeof(tradingAccountReq));
	strcpy(tradingAccountReq.BrokerID, gBrokerID);
	strcpy(tradingAccountReq.InvestorID, gInvesterID);
	strcpy_s(tradingAccountReq.CurrencyID, "CNY");
	static int requestID = 0; // 请求编号
	std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 有时候需要停顿一会才能查询成功
	int rt = g_pTradeUserApi->ReqQryTradingAccount(&tradingAccountReq, requestID);
	if (!rt) {
		tradeLog->logInfo(">>>>>>发送投资者资金账户查询请求成功");
		curReqFun = std::bind(&CustomTradeSpi::reqQueryTradingAccount, this);
	}
	else if (rt = -2 || rt == -3) {

		tradeLog->logErr("--->>>发送投资者结算结果确认请求失败");
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		reqQueryTradingAccount();
	}
	else {
		tradeLog->logErr("--->>>发送投资者资金账户查询请求失败");
		tradeLog->logErr("--->>>网络连接失败");
	}
}

void CustomTradeSpi::reqQueryInvestorPosition()
{
	CThostFtdcQryInvestorPositionField postionReq;
	memset(&postionReq, 0, sizeof(postionReq));
	strcpy(postionReq.BrokerID, gBrokerID);
	strcpy(postionReq.InvestorID, gInvesterID);
	strcpy(postionReq.InstrumentID, g_pTradeInstrumentID);
	static int requestID = 0; // 请求编号
	std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 有时候需要停顿一会才能查询成功
	int rt = g_pTradeUserApi->ReqQryInvestorPosition(&postionReq, requestID);
	if (!rt) {
		tradeLog->logInfo(">>>>>>发送投资者持仓查询请求成功");
		curReqFun = std::bind(&CustomTradeSpi::reqQueryInvestorPosition, this);
	}
	else if (rt = -2 || rt == -3) {

		tradeLog->logErr("--->>>发送投资者持仓查询请求失败");
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		reqQueryInvestorPosition();
	}
	else {
		tradeLog->logErr("--->>>发送投资者持仓查询请求失败");
		tradeLog->logErr("--->>>网络连接失败");
	}	
}

void CustomTradeSpi::reqOrderInsert()
{
	CThostFtdcInputOrderField orderInsertReq;
	memset(&orderInsertReq, 0, sizeof(CThostFtdcInputOrderField));
	///经纪公司代码
	strcpy(orderInsertReq.BrokerID, gBrokerID);
	///投资者代码
	strcpy(orderInsertReq.InvestorID, gInvesterID);
	///合约代码
	strcpy(orderInsertReq.InstrumentID, g_pTradeInstrumentID);
	///报单引用
	//strcpy(orderInsertReq.OrderRef, order_ref);
	///报单价格条件: 限价
	orderInsertReq.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
	///买卖方向: 
	orderInsertReq.Direction = THOST_FTDC_D_Buy;
	///组合开平标志: 开仓
	orderInsertReq.CombOffsetFlag[0] = THOST_FTDC_OF_Open;
	///组合投机套保标志
	orderInsertReq.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
	///价格
	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	TickToKlineHelper& tickToKlineObject = g_KlineHash.at(std::string(g_pTradeInstrumentID));
	orderInsertReq.LimitPrice = tickToKlineObject.lastPrice;
	///数量：1
	orderInsertReq.VolumeTotalOriginal = 1;
	///有效期类型: 当日有效
	orderInsertReq.TimeCondition = THOST_FTDC_TC_GFD;
	///成交量类型: 任何数量
	orderInsertReq.VolumeCondition = THOST_FTDC_VC_AV;
	///最小成交量: 1
	orderInsertReq.MinVolume = 1;
	///触发条件: 立即
	orderInsertReq.ContingentCondition = THOST_FTDC_CC_Immediately;
	orderInsertReq.StopPrice = 0;
	///强平原因: 非强平
	orderInsertReq.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
	///自动挂起标志: 否
	orderInsertReq.IsAutoSuspend = 0;
	///用户强评标志: 否
	orderInsertReq.UserForceClose = 0;
	strcpy_s(orderInsertReq.ExchangeID, InstrumentFieldMap[orderInsertReq.InstrumentID].ExchangeID);

	static int requestID = 0; // 请求编号
	int rt = g_pTradeUserApi->ReqOrderInsert(&orderInsertReq, ++requestID);
	if (!rt) {
		tradeLog->logInfo(">>>>>>发送报单录入请求成功");
		curReqFun = [this]() {this->reqOrderInsert(); };
	}
	else
		tradeLog->logErr("--->>>发送报单录入请求失败");
}

void CustomTradeSpi::reqOrderInsert(
	const TThostFtdcInstrumentIDType instrumentID,
	TThostFtdcPriceType price,
	TThostFtdcVolumeType volume,
	TThostFtdcDirectionType direction)
{
	CThostFtdcInputOrderField orderInsertReq;
	memset(&orderInsertReq, 0, sizeof(orderInsertReq));
	///经纪公司代码
	strcpy(orderInsertReq.BrokerID, gBrokerID);
	///投资者代码
	strcpy(orderInsertReq.InvestorID, gInvesterID);
	///合约代码
	strcpy(orderInsertReq.InstrumentID, instrumentID);
	///报单引用
	strcpy(orderInsertReq.OrderRef, order_ref);
	///报单价格条件: 限价
	orderInsertReq.OrderPriceType = THOST_FTDC_OPT_LimitPrice;
	///买卖方向: 
	orderInsertReq.Direction = direction;
	///组合开平标志: 开仓
	orderInsertReq.CombOffsetFlag[0] = THOST_FTDC_OF_Open;
	///组合投机套保标志
	orderInsertReq.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
	///价格
	orderInsertReq.LimitPrice = price;
	///数量：1
	orderInsertReq.VolumeTotalOriginal = volume;
	///有效期类型: 当日有效
	orderInsertReq.TimeCondition = THOST_FTDC_TC_GFD;
	///成交量类型: 任何数量
	orderInsertReq.VolumeCondition = THOST_FTDC_VC_AV;
	///最小成交量: 1
	orderInsertReq.MinVolume = 1;
	///触发条件: 立即
	orderInsertReq.ContingentCondition = THOST_FTDC_CC_Immediately;
	///强平原因: 非强平
	orderInsertReq.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
	///自动挂起标志: 否
	orderInsertReq.IsAutoSuspend = 0;
	///用户强评标志: 否
	orderInsertReq.UserForceClose = 0;

	static int requestID = 0; // 请求编号
	int rt = g_pTradeUserApi->ReqOrderInsert(&orderInsertReq, ++requestID);
	if (!rt) {
		tradeLog->logInfo(">>>>>>发送报单录入请求成功");
		std::string  instrumentStr(instrumentID);
		curReqFun = [this, instrumentStr, price, volume, direction]() {this->reqOrderInsert(instrumentStr.c_str(), price, volume, direction); };
	}
	else
		tradeLog->logErr("--->>>发送报单录入请求失败");
}

void CustomTradeSpi::reqOrder(std::shared_ptr<CThostFtdcInputOrderField> orderInsertReqPtr, bool isDefault) {
	CThostFtdcInputOrderField& orderInsertReq = *orderInsertReqPtr;
	if (isDefault) {
		strcpy(orderInsertReq.BrokerID, gBrokerID);
		///投资者代码
		strcpy(orderInsertReq.InvestorID, gInvesterID);
		strcpy(orderInsertReq.UserID, gInvesterID);
		///报单引用
		//strcpy(orderInsertReq.OrderRef, order_ref);
		///报单价格条件: 限价
		orderInsertReq.OrderPriceType = THOST_FTDC_OPT_LimitPrice; //THOST_FTDC_OPT_AnyPrice;
		///组合投机套保标志
		orderInsertReq.CombHedgeFlag[0] = THOST_FTDC_HF_Speculation;
		///有效期类型: 当日有效
		orderInsertReq.TimeCondition = THOST_FTDC_TC_GFD;
		///成交量类型: 任何数量
		orderInsertReq.VolumeCondition = THOST_FTDC_VC_AV;

		strcpy_s(orderInsertReq.ExchangeID, InstrumentFieldMap[orderInsertReq.InstrumentID].ExchangeID);
		///最小成交量: 1
		orderInsertReq.MinVolume = 1;
		///触发条件: 立即
		orderInsertReq.ContingentCondition = THOST_FTDC_CC_Immediately;
		///强平原因: 非强平
		orderInsertReq.ForceCloseReason = THOST_FTDC_FCC_NotForceClose;
		///自动挂起标志: 否
		orderInsertReq.IsAutoSuspend = 0;
		///用户强评标志: 否
		//orderInsertReq.UserForceClose = 0;

	}
	static int requestID = 0; // 请求编号
	int rt = g_pTradeUserApi->ReqOrderInsert(orderInsertReqPtr.get(), ++requestID);
	if (!rt) {
		tradeLog->logInfo(">>>>>>发送报单录入请求成功");
		curReqFun = [this, orderInsertReqPtr, isDefault]() {this->reqOrder(orderInsertReqPtr, isDefault); };
	}
	else if (rt == -2 || rt == -3) {
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		reqOrder(orderInsertReqPtr, false);
	}
	else
		tradeLog->logErr("--->>>发送报单录入请求失败" );
}

void CustomTradeSpi::reqOrderAction(CThostFtdcOrderField *pOrder)
{
	static bool orderActionSentFlag = false; // 是否发送了报单
	if (orderActionSentFlag)
		return;

	CThostFtdcInputOrderActionField orderActionReq;
	memset(&orderActionReq, 0, sizeof(orderActionReq));
	///经纪公司代码
	strcpy(orderActionReq.BrokerID, pOrder->BrokerID);
	///投资者代码
	strcpy(orderActionReq.InvestorID, pOrder->InvestorID);
	///报单操作引用
	//	TThostFtdcOrderActionRefType	OrderActionRef;
	///报单引用
	strcpy(orderActionReq.OrderRef, pOrder->OrderRef);
	///请求编号
	//	TThostFtdcRequestIDType	RequestID;
	///前置编号
	orderActionReq.FrontID = trade_front_id;
	///会话编号
	orderActionReq.SessionID = session_id;
	///交易所代码
	//	TThostFtdcExchangeIDType	ExchangeID;
	///报单编号
	//	TThostFtdcOrderSysIDType	OrderSysID;
	///操作标志
	orderActionReq.ActionFlag = THOST_FTDC_AF_Delete;
	///价格
	//	TThostFtdcPriceType	LimitPrice;
	///数量变化
	//	TThostFtdcVolumeType	VolumeChange;
	///用户代码
	//	TThostFtdcUserIDType	UserID;
	///合约代码
	strcpy(orderActionReq.InstrumentID, pOrder->InstrumentID);
	static int requestID = 0; // 请求编号
	int rt = g_pTradeUserApi->ReqOrderAction(&orderActionReq, ++requestID);
	if (!rt)
		tradeLog->logInfo(">>>>>>发送报单操作请求成功" );
	else
		tradeLog->logErr("--->>>发送报单操作请求失败" );
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