#include <vector>
#include <string>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <memory>
#include "TestStrategy.h"

extern std::unordered_map<std::string, TickToKlineHelper> test_KlineHash;


void PivotReversalStrategy::operator()()
{
	if (!opStart) {
		opStart = true;
		curVolume = 0;
	}
	if (status >= 8) {
		return;
	}
	else {
		if (taskQue.size() > 0) {
			taskQue.front()();
			taskQue.pop_front();
		}
	}
	std::lock_guard<std::mutex> lk(strategyMutex);
	double swh = pivot(Strategy::Type::high);
	double swl = pivot(Strategy::Type::low);
	if (swh <= 0.0) {
		if (highPivotQue.empty()) {
			return;
		}
		swh = highPivotQue.back();
	}

	if (swl <= 0.0) {
		if (lowPivotQue.empty()) {
			return;
		}
		swl = lowPivotQue.back();
	}
	TickToKlineHelper& tickToKlineObject = test_KlineHash.at(instrumentID);
	if (tickToKlineObject.lastPrice > swh) {
		if (status == 0) {
			this->preStatus = 0;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Open, volume);

			this->status = 8 | 1;
		}
		else if (status == 2) {
			preStatus = 2;
			{
				makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, curVolume.load());
				this->status = 8;
			}
			auto lastPrice = tickToKlineObject.lastPrice;
			taskQue.emplace_back([this, swh]() {
				this->preStatus = 0;
				std::lock_guard<std::mutex> lk(strategyMutex);
				TickToKlineHelper& tickToKlineObject = test_KlineHash.at(this->instrumentID);
				if (tickToKlineObject.lastPrice > swh) {
					makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Open, volume);
					this->status = 8 | 1;
				}
				else {
					this->status = 0;
				}

			});

		}
		else if (status == 1 && curVolume < volume) {
			this->preStatus = 1;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Open, volume - curVolume);
			this->status = 8 | 1;
		}
	}
	if (tickToKlineObject.lastPrice < swl) {
		if (status == 0) {
			preStatus = 0;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_Open, volume);
			status = 8 | 2;
		}
		else if (status == 1) {
			this->preStatus = 1;
			{
				makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, curVolume.load());
				this->status = 8;
			}
			auto lastPrice = tickToKlineObject.lastPrice;
			taskQue.emplace_back([this, swl]() {
				std::lock_guard<std::mutex> lk(strategyMutex);
				this->preStatus = 0;
				TickToKlineHelper& tickToKlineObject = test_KlineHash.at(this->instrumentID);
				if (tickToKlineObject.lastPrice < swl) {
					makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_Open, volume);
					this->status = 8 | 2;
				}
				else {
					this->status = 0;
				}
			});

		}
		else if (status == 2 && curVolume < volume) {
			this->preStatus = 2;
			this->status = 8 | 2;
			makeOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Open, volume - curVolume);
		}
	}
}

void PivotReversalStrategy::clearInvestor(CThostFtdcInvestorPositionField investor, int status, bool isLast) {
	if (status <= 0) {
		return;
	}
	tasks.emplace_back([this, investor, status, isLast]() {

		int limit = instrumentField.MaxMarketOrderVolume / 3;
		limit = limit > instrumentField.MinMarketOrderVolume ? limit : instrumentField.MinMarketOrderVolume;
		std::lock_guard<std::mutex> lk(strategyMutex);

		TickToKlineHelper& tickToKlineObject = test_KlineHash.at(instrumentID);
		if ((status & 1) != 0) {
			int YdPosition = investor.Position - investor.TodayPosition;
			if (investor.PosiDirection == THOST_FTDC_PD_Long) {
				this->longInvestor = investor;
				if (instrumentField.MaxMarketOrderVolume < YdPosition) {
					for (int p = YdPosition; p > 0; p -= limit) {
						std::this_thread::sleep_for(std::chrono::milliseconds(1000));
						makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Close, p > limit ? limit : p);
					}
				}
				else if (YdPosition > 0) {
					makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_Close, YdPosition);
				}
			}
			else if (investor.PosiDirection == THOST_FTDC_PD_Short) {
				if (instrumentField.MaxMarketOrderVolume < YdPosition) {
					for (int p = YdPosition; p > 0; p -= limit) {
						std::this_thread::sleep_for(std::chrono::milliseconds(1000));
						makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_Close, p > limit ? limit : p);
					}
				}
				else if (YdPosition > 0) {
					makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_Close, YdPosition);
				}

			}
		}
		if ((status & 2) != 0) {
			if (investor.PosiDirection == THOST_FTDC_PD_Long) {
				this->longInvestor = investor;
				if (instrumentField.MaxMarketOrderVolume < investor.TodayPosition) {
					for (int p = investor.TodayPosition; p > 0; p -= limit) {
						std::this_thread::sleep_for(std::chrono::milliseconds(1000));
						makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, p > limit ? limit : p);
					}

				}
				else if (investor.TodayPosition > 0) {
					makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Buy, THOST_FTDC_OF_CloseToday, investor.TodayPosition);
				}
			}
			else if (investor.PosiDirection == THOST_FTDC_PD_Short) {
				shortInvestor = investor;
				if (instrumentField.MaxMarketOrderVolume < investor.TodayPosition) {
					for (int p = investor.TodayPosition; p > 0; p -= limit) {
						std::this_thread::sleep_for(std::chrono::milliseconds(1000));
						makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, p > limit ? limit : p);
					}
				}
				else if (investor.TodayPosition > 0) {
					makeClearOrder(tickToKlineObject.lastPrice, THOST_FTDC_D_Sell, THOST_FTDC_OF_CloseToday, investor.TodayPosition);
				}

			}
		}

		if (!last) {
			initVolume = initVolume + investor.Position + investor.YdPosition;
		}
		if (isLast) {
			last = true;
		}
		MvStatus = 1;
	});
}

double PivotReversalStrategy::pivot(Strategy::Type type) {
	int range = left + right;
	std::vector<double> pivotArray;
	TickToKlineHelper& tickToKlineObject = test_KlineHash.at(instrumentID);
	if (tickToKlineObject.m_KLineDataArray.size() < range) {
		return 0.0;
	}

	size_t size = tickToKlineObject.m_KLineDataArray.size();
	bool isMin = false;
	switch (type) {
	case Strategy::Type::high:
	{
		if (size == barsNumHigh) {
			return 0.0;
		}
		else {
			barsNumHigh = size;
		}
		int i = range;
		for (auto it = tickToKlineObject.m_KLineDataArray.rbegin(); i > 0; i--, it++) {
			pivotArray.push_back(it->high_price);
		}
		break;
	}
	case Strategy::Type::low:
	{
		if (size == barsNumLow) {
			return 0.0;
		}
		else {
			barsNumLow = size;
		}
		int i = range;
		for (auto it = tickToKlineObject.m_KLineDataArray.rbegin(); i > 0; i--, it++) {
			pivotArray.push_back(it->low_price);
		}
		isMin = true;
		break;
	}
	case Strategy::Type::open:
		break;
	case Strategy::Type::close:
		break;
	}
	double pivotVal = pivotArray[right];
	for (int i = 0; i < range; i++) {
		if (i < right) {
			if (isMin) {
				if (pivotVal >= pivotArray[i] || fabs(pivotVal - pivotArray[i]) < 0.0005) {
					return 0.0;
				}
			}
			else {
				if (pivotVal <= pivotArray[i] || fabs(pivotVal - pivotArray[i]) < 0.0005) {
					return 0.0;
				}
			}
		}
		else if (i > right) {
			if (isMin) {
				if (pivotVal > pivotArray[i]) {
					return 0.0;
				}
			}
			else {
				if (pivotVal < pivotArray[i]) {
					return 0.0;
				}
			}
		}
	}
	if (!isMin) {
		highPivotQue.push_back(pivotVal);
		outFile << "high:" << pivotVal << std::endl;
		if (highPivotQue.size() > 100) {
			highPivotQue.pop_front();
		}
	}
	else {
		lowPivotQue.push_back(pivotVal);
		outFile << "low:" << pivotVal << std::endl;
		if (lowPivotQue.size() > 100) {
			lowPivotQue.pop_front();
		}
	}

	return pivotVal;
}
