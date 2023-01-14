#pragma once
// ---- ����k�ߵ��� ---- //

#include <vector>
#include <string>
#include <list>
#include "log.h"

// k�����ݽṹ
struct KLineDataType
{
	double open_price;   // ��
	double high_price;   // ��
	double low_price;    // ��
	double close_price;  // ��
	int volume;          // ��
};

class TickToKlineHelper
{
public:
	// �ӱ������ݹ���k�ߣ����洢������(�ٶ���������û�ж���)
	void KLineFromLocalData(const std::string &sFilePath, const std::string &dFilePath); 
	// ��ʵʱ���ݹ���k��
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
	std::vector<double> m_priceVec; // �洢5���ӵļ۸�
	std::vector<int> m_volumeVec; // �洢5���ӵĳɽ���
	std::list<KLineDataType> m_KLineDataArray;
	std::list<double> newPrices;
	int kData = 0;
	std::string instrument;
	time_t cur_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	volatile double lastPrice;
	std::ofstream outFile;
};
