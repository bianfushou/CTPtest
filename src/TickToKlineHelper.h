#pragma once
// ---- 计算k线的类 ---- //

#include <vector>
#include <string>
#include <list>
#include "log.h"

// k线数据结构
struct KLineDataType
{
	double open_price;   // 开
	double high_price;   // 高
	double low_price;    // 低
	double close_price;  // 收
	int volume;          // 量
};

class TickToKlineHelper
{
public:
	// 从本地数据构建k线，并存储到本地(假定本地数据没有丢包)
	void KLineFromLocalData(const std::string &sFilePath, const std::string &dFilePath); 
	// 从实时数据构建k线
	void KLineFromRealtimeData(CThostFtdcDepthMarketDataField *pDepthMarketData);

	double getNewAvgPrice() {
		double sum = 0;
		for (double d : newPrices) {
			sum += d;
		}
		return sum / newPrices.size();
	}
	double getNewMinPrice() {
		auto iter = newPrices.crbegin();
		double minP = *iter;
		iter++;
		for (int i = 0; i < newPrices.size() / 2; i++, iter++) {
			if (minP > *iter) {
				minP = *iter;
			}
		}
		return minP;
	 }
public:
	bool isRecord = false;
	bool isInit = false;
	std::vector<double> m_priceVec; // 存储5分钟的价格
	std::vector<int> m_volumeVec; // 存储5分钟的成交量
	std::list<KLineDataType> m_KLineDataArray;
	std::list<double> newPrices;
	int kData = 0;
	std::string instrument;
	time_t cur_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	volatile double lastPrice;
	std::ofstream outFile;
};
