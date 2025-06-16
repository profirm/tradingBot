// Prevent Windows headers from defining min/max macros
#define NOMINMAX
#include "sierrachart.h"
#undef max
#undef min
#include <vector>
#include <map>
#include <string>
#include <algorithm>
#include <numeric>
#include <memory>
#include <cmath>

SCDLLName("Advanced Order Flow Trading Bot v2.0")

// ==================================================================================
// FORWARD DECLARATIONS & STRUCTURES
// ==================================================================================

struct TradeSignal {
    int direction;          // 1 = Long, -1 = Short, 0 = No Signal
    float confidence;       // 0.0 to 1.0
    std::string strategy;   // Strategy name
    float entryPrice;
    float stopLoss;
    float target;
    std::string reason;
};

struct StrategyConfig {
    bool isEnabled;
    float weightMultiplier;
    std::string name;
};

struct RiskMetrics {
    float dailyPnL;
    float maxDrawdown;
    float portfolioHeat;
    int tradesTotal;
    int tradesWin;
    int tradesLoss;
    float winRate;
    float profitFactor;
    float largestWin;
    float largestLoss;
};

struct VolumeProfileLevel {
    float price;
    float volume;
    bool isHVN;
    bool isLVN;
};

struct OrderFlowData {
    float cumulativeDelta;
    float deltaMA;
    float volumeImbalance;
    float absorptionStrength;
    std::vector<VolumeProfileLevel> profileLevels;
};

// Strategy Function Declarations
TradeSignal CheckLiquidityAbsorption(SCStudyInterfaceRef sc, int index);
TradeSignal CheckIcebergDetection(SCStudyInterfaceRef sc, int index);
TradeSignal CheckDeltaDivergence(SCStudyInterfaceRef sc, int index);
TradeSignal CheckVolumeImbalance(SCStudyInterfaceRef sc, int index);
TradeSignal CheckStopRunAnticipation(SCStudyInterfaceRef sc, int index);
TradeSignal CheckHVNRejection(SCStudyInterfaceRef sc, int index);
TradeSignal CheckLVNBreakout(SCStudyInterfaceRef sc, int index);
TradeSignal CheckMomentumBreakout(SCStudyInterfaceRef sc, int index);
TradeSignal CheckCumulativeDeltaTrend(SCStudyInterfaceRef sc, int index);
TradeSignal CheckLiquidityTraps(SCStudyInterfaceRef sc, int index);

// Utility Functions
void LogTrade(SCStudyInterfaceRef sc, const TradeSignal& signal, const std::string& action);
void UpdateRiskMetrics(SCStudyInterfaceRef sc, RiskMetrics& metrics);
float CalculatePositionSize(SCStudyInterfaceRef sc, const TradeSignal& signal, const RiskMetrics& metrics);
bool ValidateSignal(SCStudyInterfaceRef sc, const TradeSignal& signal);
void ProcessVolumeProfile(SCStudyInterfaceRef sc);
void UpdateOrderFlowData(SCStudyInterfaceRef sc);
bool IsWithinTradingHours(SCStudyInterfaceRef sc);
float CalculateVolatility(SCStudyInterfaceRef sc, int lookback);
std::vector<float> FindSwingPoints(SCStudyInterfaceRef sc, int lookback, bool findHighs);

// ==================================================================================
// MAIN STUDY FUNCTION
// ==================================================================================

SCSFExport scsf_AdvancedOrderFlowBot(SCStudyInterfaceRef sc)
{
    // ===============================================================================
    // STUDY CONFIGURATION & INITIALIZATION
    // ===============================================================================
    
    if (sc.SetDefaults)
    {
        sc.GraphName = "Advanced Order Flow Trading Bot v2.0";
        sc.StudyDescription = "Professional multi-strategy order flow trading system with advanced risk management";
        sc.AutoLoop = 0;  // Manual loop for tick-by-tick analysis
        sc.GraphRegion = 0;
        sc.IsAutoTradingEnabled = 1;
        sc.MaintainVolumeAtPriceData = 1;
        sc.CalculationPrecedence = LOW_PREC_LEVEL;

        // ===============================================================================
        // SUBGRAPHS FOR VISUALIZATION
        // ===============================================================================
        
        sc.Subgraph[0].Name = "Cumulative Delta";
        sc.Subgraph[0].DrawStyle = DRAWSTYLE_LINE;
        sc.Subgraph[0].PrimaryColor = RGB(255, 255, 0);
        sc.Subgraph[0].LineWidth = 2;

        sc.Subgraph[1].Name = "Delta Moving Average";
        sc.Subgraph[1].DrawStyle = DRAWSTYLE_LINE;
        sc.Subgraph[1].PrimaryColor = RGB(0, 255, 255);
        sc.Subgraph[1].LineWidth = 1;

        sc.Subgraph[2].Name = "Absorption Signals";
        sc.Subgraph[2].DrawStyle = DRAWSTYLE_ARROWUP;
        sc.Subgraph[2].PrimaryColor = RGB(0, 255, 0);
        sc.Subgraph[2].SecondaryColor = RGB(255, 0, 0);
        sc.Subgraph[2].DrawZeros = false;
        sc.Subgraph[2].LineWidth = 3;

        sc.Subgraph[3].Name = "Iceberg Signals";
        sc.Subgraph[3].DrawStyle = DRAWSTYLE_SQUARE;
        sc.Subgraph[3].PrimaryColor = RGB(255, 165, 0);
        sc.Subgraph[3].SecondaryColor = RGB(255, 69, 0);
        sc.Subgraph[3].DrawZeros = false;
        sc.Subgraph[3].LineWidth = 2;

        sc.Subgraph[4].Name = "Volume Imbalance";
        sc.Subgraph[4].DrawStyle = DRAWSTYLE_DIAMOND;
        sc.Subgraph[4].PrimaryColor = RGB(138, 43, 226);
        sc.Subgraph[4].SecondaryColor = RGB(255, 20, 147);
        sc.Subgraph[4].DrawZeros = false;
        sc.Subgraph[4].LineWidth = 2;

        sc.Subgraph[5].Name = "Stop Run Signals";
        sc.Subgraph[5].DrawStyle = DRAWSTYLE_PLUS;
        sc.Subgraph[5].PrimaryColor = RGB(255, 215, 0);
        sc.Subgraph[5].SecondaryColor = RGB(255, 140, 0);
        sc.Subgraph[5].DrawZeros = false;
        sc.Subgraph[5].LineWidth = 4;

        sc.Subgraph[6].Name = "HVN Levels";
        sc.Subgraph[6].DrawStyle = DRAWSTYLE_DASH;
        sc.Subgraph[6].PrimaryColor = RGB(255, 0, 0);
        sc.Subgraph[6].DrawZeros = false;
        sc.Subgraph[6].LineWidth = 2;

        sc.Subgraph[7].Name = "LVN Levels";
        sc.Subgraph[7].DrawStyle = DRAWSTYLE_POINT; // was DRAWSTYLE_DOT, use valid style
        sc.Subgraph[7].PrimaryColor = RGB(128, 128, 128);
        sc.Subgraph[7].DrawZeros = false;
        sc.Subgraph[7].LineWidth = 1;

        sc.Subgraph[8].Name = "Portfolio Heat";
        sc.Subgraph[8].DrawStyle = DRAWSTYLE_BAR;
        sc.Subgraph[8].PrimaryColor = RGB(255, 99, 71);
        sc.Subgraph[8].DrawZeros = false;

        sc.Subgraph[9].Name = "Strategy Signals Combined";
        sc.Subgraph[9].DrawStyle = DRAWSTYLE_TRIANGLEUP;
        sc.Subgraph[9].PrimaryColor = RGB(50, 205, 50);
        sc.Subgraph[9].SecondaryColor = RGB(220, 20, 60);
        sc.Subgraph[9].DrawZeros = false;
        sc.Subgraph[9].LineWidth = 3;

        // ===============================================================================
        // MASTER SYSTEM CONTROLS
        // ===============================================================================
        
        sc.Input[0].Name = "=== MASTER SYSTEM CONTROLS ===";
        sc.Input[0].SetDescription("Primary system configuration settings");
        
        sc.Input[1].Name = "Enable Auto Trading";
        sc.Input[1].SetYesNo(false);
        sc.Input[1].SetDescription("Master switch for automated trading execution");

        sc.Input[2].Name = "Trade Quantity";
        sc.Input[2].SetInt(1);
        sc.Input[2].SetIntLimits(1, 100);
        sc.Input[2].SetDescription("Base position size for trades");

        sc.Input[3].Name = "Max Daily Trades";
        sc.Input[3].SetInt(20);
        sc.Input[3].SetIntLimits(1, 100);
        sc.Input[3].SetDescription("Maximum number of trades per day");

        sc.Input[4].Name = "Enable Detailed Logging";
        sc.Input[4].SetYesNo(true);
        sc.Input[4].SetDescription("Log detailed trading decisions and analysis");

        sc.Input[5].Name = "Run All Strategies";
        sc.Input[5].SetYesNo(false);
        sc.Input[5].SetDescription("Override individual strategy settings and enable all");

        // ===============================================================================
        // RISK MANAGEMENT CONTROLS
        // ===============================================================================
        
        sc.Input[10].Name = "=== RISK MANAGEMENT ===";
        sc.Input[10].SetDescription("Risk control and position sizing parameters");

        sc.Input[11].Name = "Max Daily Loss ($)";
        sc.Input[11].SetFloat(1000.0f);
        sc.Input[11].SetFloatLimits(100.0f, 10000.0f);
        sc.Input[11].SetDescription("Maximum allowed daily loss in dollars");

        sc.Input[12].Name = "Daily Profit Target ($)";
        sc.Input[12].SetFloat(2000.0f);
        sc.Input[12].SetFloatLimits(100.0f, 20000.0f);
        sc.Input[12].SetDescription("Daily profit target - stop trading when reached");

        sc.Input[13].Name = "Max Portfolio Heat (%)";
        sc.Input[13].SetFloat(2.0f);
        sc.Input[13].SetFloatLimits(0.5f, 10.0f);
        sc.Input[13].SetDescription("Maximum portfolio risk as percentage");

        sc.Input[14].Name = "Position Size Risk (%)";
        sc.Input[14].SetFloat(1.0f);
        sc.Input[14].SetFloatLimits(0.1f, 5.0f);
        sc.Input[14].SetDescription("Risk per trade as percentage of account");

        sc.Input[15].Name = "Max Drawdown Limit (%)";
        sc.Input[15].SetFloat(5.0f);
        sc.Input[15].SetFloatLimits(1.0f, 20.0f);
        sc.Input[15].SetDescription("Maximum allowed drawdown before shutdown");

        // ===============================================================================
        // TIME-BASED CONTROLS
        // ===============================================================================
        
        sc.Input[20].Name = "=== TIME CONTROLS ===";
        sc.Input[20].SetDescription("Trading session and time-based restrictions");

        sc.Input[21].Name = "Trading Start Time";
        sc.Input[21].SetTime(HMS_TIME(9, 30, 0));
        sc.Input[21].SetDescription("Daily trading session start time");

        sc.Input[22].Name = "Trading End Time";
        sc.Input[22].SetTime(HMS_TIME(15, 45, 0));
        sc.Input[22].SetDescription("Daily trading session end time");

        sc.Input[23].Name = "Force Flatten Time";
        sc.Input[23].SetTime(HMS_TIME(15, 55, 0));
        sc.Input[23].SetDescription("Time to force close all positions");

        sc.Input[24].Name = "Avoid First N Minutes";
        sc.Input[24].SetInt(15);
        sc.Input[24].SetIntLimits(0, 60);
        sc.Input[24].SetDescription("Minutes to avoid trading after market open");

        sc.Input[25].Name = "Avoid Last N Minutes";
        sc.Input[25].SetInt(15);
        sc.Input[25].SetIntLimits(0, 60);
        sc.Input[25].SetDescription("Minutes to avoid trading before market close");

        // ===============================================================================
        // STRATEGY ENABLE/DISABLE CONTROLS
        // ===============================================================================
        
        sc.Input[30].Name = "=== STRATEGY CONTROLS ===";
        sc.Input[30].SetDescription("Individual strategy activation settings");

        sc.Input[31].Name = "Enable Liquidity Absorption";
        sc.Input[31].SetYesNo(true);
        sc.Input[31].SetDescription("Detect and trade absorption patterns");

        sc.Input[32].Name = "Enable Iceberg Detection";
        sc.Input[32].SetYesNo(true);
        sc.Input[32].SetDescription("Detect and trade iceberg orders");

        sc.Input[33].Name = "Enable Delta Divergence";
        sc.Input[33].SetYesNo(true);
        sc.Input[33].SetDescription("Trade delta divergence signals");

        sc.Input[34].Name = "Enable Volume Imbalance";
        sc.Input[34].SetYesNo(true);
        sc.Input[34].SetDescription("Trade footprint volume imbalances");

        sc.Input[35].Name = "Enable Stop Run Anticipation";
        sc.Input[35].SetYesNo(true);
        sc.Input[35].SetDescription("Anticipate and trade stop runs");

        sc.Input[36].Name = "Enable HVN Rejection";
        sc.Input[36].SetYesNo(true);
        sc.Input[36].SetDescription("Trade rejections from High Volume Nodes");

        sc.Input[37].Name = "Enable LVN Breakout";
        sc.Input[37].SetYesNo(true);
        sc.Input[37].SetDescription("Trade breakouts through Low Volume Nodes");

        sc.Input[38].Name = "Enable Momentum Breakout";
        sc.Input[38].SetYesNo(true);
        sc.Input[38].SetDescription("Trade momentum-confirmed breakouts");

        sc.Input[39].Name = "Enable Cumulative Delta";
        sc.Input[39].SetYesNo(true);
        sc.Input[39].SetDescription("Use cumulative delta for trend confirmation");

        sc.Input[40].Name = "Enable Liquidity Traps";
        sc.Input[40].SetYesNo(true);
        sc.Input[40].SetDescription("Detect and fade liquidity traps");

        // ===============================================================================
        // STRATEGY PARAMETERS - LIQUIDITY ABSORPTION
        // ===============================================================================
        
        sc.Input[50].Name = "=== ABSORPTION PARAMETERS ===";
        sc.Input[50].SetDescription("Liquidity absorption strategy settings");

        sc.Input[51].Name = "Absorption Volume Threshold";
        sc.Input[51].SetInt(100);
        sc.Input[51].SetIntLimits(10, 1000);
        sc.Input[51].SetDescription("Minimum volume for absorption detection");

        sc.Input[52].Name = "Absorption Price Stall (Ticks)";
        sc.Input[52].SetInt(3);
        sc.Input[52].SetIntLimits(1, 10);
        sc.Input[52].SetDescription("Maximum price movement during absorption");

        sc.Input[53].Name = "Absorption Confirmation Bars";
        sc.Input[53].SetInt(2);
        sc.Input[53].SetIntLimits(1, 5);
        sc.Input[53].SetDescription("Bars needed to confirm absorption");

        // ===============================================================================
        // STRATEGY PARAMETERS - ICEBERG DETECTION
        // ===============================================================================
        
        sc.Input[60].Name = "=== ICEBERG PARAMETERS ===";
        sc.Input[60].SetDescription("Iceberg order detection settings");

        sc.Input[61].Name = "Iceberg Min Hit Volume";
        sc.Input[61].SetInt(50);
        sc.Input[61].SetIntLimits(10, 500);
        sc.Input[61].SetDescription("Minimum volume per iceberg hit");

        sc.Input[62].Name = "Iceberg Detection Bars";
        sc.Input[62].SetInt(5);
        sc.Input[62].SetIntLimits(3, 20);
        sc.Input[62].SetDescription("Bars to analyze for iceberg pattern");

        sc.Input[63].Name = "Iceberg Price Tolerance (Ticks)";
        sc.Input[63].SetInt(1);
        sc.Input[63].SetIntLimits(0, 3);
        sc.Input[63].SetDescription("Price tolerance for iceberg level");

        // ===============================================================================
        // STRATEGY PARAMETERS - DELTA ANALYSIS
        // ===============================================================================
        
        sc.Input[70].Name = "=== DELTA PARAMETERS ===";
        sc.Input[70].SetDescription("Delta analysis and divergence settings");

        sc.Input[71].Name = "Delta MA Period";
        sc.Input[71].SetInt(20);
        sc.Input[71].SetIntLimits(5, 100);
        sc.Input[71].SetDescription("Moving average period for delta smoothing");

        sc.Input[72].Name = "Divergence Lookback Period";
        sc.Input[72].SetInt(20);
        sc.Input[72].SetIntLimits(10, 50);
        sc.Input[72].SetDescription("Bars to look back for divergence analysis");

        sc.Input[73].Name = "Delta Exhaustion Threshold";
        sc.Input[73].SetFloat(2.0f);
        sc.Input[73].SetFloatLimits(1.0f, 5.0f);
        sc.Input[73].SetDescription("Standard deviations for delta exhaustion");

        // ===============================================================================
        // STRATEGY PARAMETERS - VOLUME PROFILE
        // ===============================================================================
        
        sc.Input[80].Name = "=== VOLUME PROFILE PARAMETERS ===";
        sc.Input[80].SetDescription("Volume profile and HVN/LVN settings");

        sc.Input[81].Name = "HVN Threshold Multiplier";
        sc.Input[81].SetFloat(2.0f);
        sc.Input[81].SetFloatLimits(1.2f, 5.0f);
        sc.Input[81].SetDescription("Multiple of average volume for HVN identification");

        sc.Input[82].Name = "LVN Threshold Multiplier";
        sc.Input[82].SetFloat(0.3f);
        sc.Input[82].SetFloatLimits(0.1f, 0.8f);
        sc.Input[82].SetDescription("Multiple of average volume for LVN identification");

        sc.Input[83].Name = "Profile Lookback Bars";
        sc.Input[83].SetInt(500);
        sc.Input[83].SetIntLimits(100, 2000);
        sc.Input[83].SetDescription("Bars to include in volume profile calculation");

        sc.Input[84].Name = "Level Proximity (Ticks)";
        sc.Input[84].SetInt(2);
        sc.Input[84].SetIntLimits(1, 10);
        sc.Input[84].SetDescription("Price proximity to HVN/LVN for signal");

        // ===============================================================================
        // STRATEGY PARAMETERS - BREAKOUT & MOMENTUM
        // ===============================================================================
        
        sc.Input[90].Name = "=== BREAKOUT PARAMETERS ===";
        sc.Input[90].SetDescription("Breakout and momentum strategy settings");

        sc.Input[91].Name = "Breakout Volume Multiplier";
        sc.Input[91].SetFloat(1.5f);
        sc.Input[91].SetFloatLimits(1.1f, 3.0f);
        sc.Input[91].SetDescription("Volume multiple required for breakout confirmation");

        sc.Input[92].Name = "Breakout Lookback Period";
        sc.Input[92].SetInt(20);
        sc.Input[92].SetIntLimits(10, 50);
        sc.Input[92].SetDescription("Bars for breakout level calculation");

        sc.Input[93].Name = "Momentum Confirmation Period";
        sc.Input[93].SetInt(5);
        sc.Input[93].SetIntLimits(2, 10);
        sc.Input[93].SetDescription("Bars needed for momentum confirmation");

        // Initialize persistent data structures
        sc.SetPersistentPointer(1, new std::vector<float>());      // HVN Levels
        sc.SetPersistentPointer(2, new std::vector<float>());      // LVN Levels
        sc.SetPersistentPointer(3, new RiskMetrics());             // Risk tracking
        sc.SetPersistentPointer(4, new std::map<std::string, int>()); // Strategy counters
        sc.SetPersistentPointer(5, new OrderFlowData());           // Order flow data

        // Initialize persistent variables
        sc.SetPersistentFloat(1, 0.0f);  // Daily P&L
        sc.SetPersistentFloat(2, 0.0f);  // Session high
        sc.SetPersistentFloat(3, 0.0f);  // Session low
        sc.SetPersistentFloat(4, 0.0f);  // Cumulative Delta
        sc.SetPersistentInt(1, 0);       // Daily trade count
        sc.SetPersistentInt(2, 0);       // Trading enabled flag
        sc.SetPersistentInt(3, 0);       // Last processed bar

        return;
    }

    // ===============================================================================
    // CLEANUP ON STUDY REMOVAL
    // ===============================================================================
    
    if (sc.LastCallToFunction)
    {
        delete (std::vector<float>*)sc.GetPersistentPointer(1);
        delete (std::vector<float>*)sc.GetPersistentPointer(2);
        delete (RiskMetrics*)sc.GetPersistentPointer(3);
        delete (std::map<std::string, int>*)sc.GetPersistentPointer(4);
        delete (OrderFlowData*)sc.GetPersistentPointer(5);
        return;
    }

    // ===============================================================================
    // MAIN TRADING LOGIC EXECUTION (PER-BAR LOOP)
    // ===============================================================================

    // Get persistent data
    std::vector<float>* hvnLevels = (std::vector<float>*)sc.GetPersistentPointer(1);
    std::vector<float>* lvnLevels = (std::vector<float>*)sc.GetPersistentPointer(2);
    RiskMetrics* riskMetrics = (RiskMetrics*)sc.GetPersistentPointer(3);
    std::map<std::string, int>* strategyCounts = (std::map<std::string, int>*)sc.GetPersistentPointer(4);
    OrderFlowData* orderFlowData = (OrderFlowData*)sc.GetPersistentPointer(5);

    if (!hvnLevels || !lvnLevels || !riskMetrics || !strategyCounts || !orderFlowData) return;

    int loopStart = sc.UpdateStartIndex;
    if (loopStart < 0) loopStart = 0;
    for (int i = loopStart; i < sc.ArraySize; ++i)
    {
        // Daily reset logic
        if (sc.IsNewTradingDay(i))
        {
            riskMetrics->dailyPnL = 0.0f;
            riskMetrics->tradesTotal = 0;
            riskMetrics->tradesWin = 0;
            riskMetrics->tradesLoss = 0;
            sc.SetPersistentInt(1, 0); // Reset daily trade count
            sc.SetPersistentInt(2, 1); // Enable trading for new day
            sc.SetPersistentFloat(4, 0.0f); // Reset cumulative delta
            strategyCounts->clear();
            if (sc.Input[4].GetYesNo())
            {
                SCString logMsg;
                logMsg.Format("=== NEW TRADING DAY === Risk limits reset. Max Loss: $%.2f, Target: $%.2f", 
                             sc.Input[11].GetFloat(), sc.Input[12].GetFloat());
                sc.AddMessageToLog(logMsg, 0);
            }
        }

        // Update risk metrics
        UpdateRiskMetrics(sc, *riskMetrics);

        // Check if trading is disabled for the day
        int tradingEnabled = sc.GetPersistentInt(2);
        if (!tradingEnabled) continue;

        // Risk limit checks
        if (riskMetrics->dailyPnL <= -sc.Input[11].GetFloat() || riskMetrics->dailyPnL >= sc.Input[12].GetFloat())
        {
            sc.SetPersistentInt(2, 0); // Disable trading
            s_SCPositionData positionData;
            sc.GetTradePosition(positionData);
            if (positionData.PositionQuantity != 0)
                sc.FlattenPosition();
            if (sc.Input[4].GetYesNo())
            {
                SCString logMsg;
                if (riskMetrics->dailyPnL <= -sc.Input[11].GetFloat())
                    logMsg.Format("DAILY LOSS LIMIT HIT: $%.2f. Trading disabled for remainder of session.", riskMetrics->dailyPnL);
                else
                    logMsg.Format("DAILY PROFIT TARGET HIT: $%.2f. Trading disabled for remainder of session.", riskMetrics->dailyPnL);
                sc.AddMessageToLog(logMsg, 0);
            }
            continue;
        }

        // Time-based trading controls
        if (!IsWithinTradingHours(sc)) continue;

        // Force flatten positions at end of day
        SCDateTime currentTime = sc.BaseDateTimeIn[i];
        SCDateTime flattenTime = sc.Input[23].GetTime();
        if (currentTime.GetTime() >= flattenTime.GetTime())
        {
            s_SCPositionData positionData;
            sc.GetTradePosition(positionData);
            if (positionData.PositionQuantity != 0)
            {
                sc.FlattenPosition();
                if (sc.Input[4].GetYesNo())
                    sc.AddMessageToLog("FORCE FLATTEN: End of trading session", 0);
            }
            continue;
        }

        // Check if already in position
        s_SCPositionData positionData;
        sc.GetTradePosition(positionData);
        bool hasPosition = (positionData.PositionQuantity != 0);

        // Check daily trade limit
        int dailyTrades = sc.GetPersistentInt(1);
        if (dailyTrades >= sc.Input[3].GetInt())
        {
            if (sc.Input[4].GetYesNo() && dailyTrades == sc.Input[3].GetInt())
            {
                sc.AddMessageToLog("DAILY TRADE LIMIT REACHED. No new positions until tomorrow.", 0);
            }
            continue;
        }

        // Update order flow data and volume profile on new bars
        if (sc.IsNewBar(i))
        {
            UpdateOrderFlowData(sc);
            ProcessVolumeProfile(sc);
        }

        // Skip if already in position (unless we want to add to positions)
        if (hasPosition) continue;

        // ===============================================================================
        // STRATEGY SIGNAL GENERATION
        // ===============================================================================
        std::vector<TradeSignal> signals;
        bool runAllStrategies = sc.Input[5].GetYesNo();
        if (runAllStrategies || sc.Input[31].GetYesNo())
        {
            TradeSignal signal = CheckLiquidityAbsorption(sc, i);
            if (signal.direction != 0) signals.push_back(signal);
        }
        if (runAllStrategies || sc.Input[32].GetYesNo())
        {
            TradeSignal signal = CheckIcebergDetection(sc, i);
            if (signal.direction != 0) signals.push_back(signal);
        }
        if (runAllStrategies || sc.Input[33].GetYesNo())
        {
            TradeSignal signal = CheckDeltaDivergence(sc, i);
            if (signal.direction != 0) signals.push_back(signal);
        }
        if (runAllStrategies || sc.Input[34].GetYesNo())
        {
            TradeSignal signal = CheckVolumeImbalance(sc, i);
            if (signal.direction != 0) signals.push_back(signal);
        }
        if (runAllStrategies || sc.Input[35].GetYesNo())
        {
            TradeSignal signal = CheckStopRunAnticipation(sc, i);
            if (signal.direction != 0) signals.push_back(signal);
        }
        if (runAllStrategies || sc.Input[36].GetYesNo())
        {
            TradeSignal signal = CheckHVNRejection(sc, i);
            if (signal.direction != 0) signals.push_back(signal);
        }
        if (runAllStrategies || sc.Input[37].GetYesNo())
        {
            TradeSignal signal = CheckLVNBreakout(sc, i);
            if (signal.direction != 0) signals.push_back(signal);
        }
        if (runAllStrategies || sc.Input[38].GetYesNo())
        {
            TradeSignal signal = CheckMomentumBreakout(sc, i);
            if (signal.direction != 0) signals.push_back(signal);
        }
        if (runAllStrategies || sc.Input[39].GetYesNo())
        {
            TradeSignal signal = CheckCumulativeDeltaTrend(sc, i);
            if (signal.direction != 0) signals.push_back(signal);
        }
        if (runAllStrategies || sc.Input[40].GetYesNo())
        {
            TradeSignal signal = CheckLiquidityTraps(sc, i);
            if (signal.direction != 0) signals.push_back(signal);
        }

        // ===============================================================================
        // SIGNAL PROCESSING AND EXECUTION
        // ===============================================================================
        if (!signals.empty())
        {
            TradeSignal bestSignal = *std::max_element(signals.begin(), signals.end(),
                [](const TradeSignal& a, const TradeSignal& b) {
                    return a.confidence < b.confidence;
                });
            if (ValidateSignal(sc, bestSignal))
            {
                float positionSize = CalculatePositionSize(sc, bestSignal, *riskMetrics);
                if (positionSize > 0)
                {
                    s_SCNewOrder order;
                    order.OrderQuantity = static_cast<int>(positionSize);
                    order.OrderType = SCT_ORDERTYPE_MARKET;
                    order.TimeInForce = SCT_TIF_GOOD_TILL_CANCELED;
                    order.Stop1Offset = std::abs(bestSignal.entryPrice - bestSignal.stopLoss);
                    order.Target1Offset = std::abs(bestSignal.target - bestSignal.entryPrice);
                    int orderResult = 0;
                    if (bestSignal.direction == 1)
                    {
                        orderResult = sc.BuyEntry(order);
                        sc.Subgraph[9][i] = sc.Low[i] - sc.TickSize;
                        sc.Subgraph[9].DataColor[i] = sc.Subgraph[9].PrimaryColor;
                    }
                    else if (bestSignal.direction == -1)
                    {
                        orderResult = sc.SellEntry(order);
                        sc.Subgraph[9][i] = sc.High[i] + sc.TickSize;
                        sc.Subgraph[9].DataColor[i] = sc.Subgraph[9].SecondaryColor;
                    }
                    if (orderResult > 0)
                    {
                        sc.SetPersistentInt(1, dailyTrades + 1);
                        (*strategyCounts)[bestSignal.strategy]++;
                        LogTrade(sc, bestSignal, "ENTRY");
                    }
                }
            }
        }
        // === Plot close price for every bar (for visual debug, optional) ===
        // sc.Subgraph[0][i] = sc.Close[i];
    }
    // END MAIN PER-BAR LOOP
}


// ===============================================================================
// UTILITY FUNCTION IMPLEMENTATIONS
// ===============================================================================

void UpdateOrderFlowData(SCStudyInterfaceRef sc)
{
    OrderFlowData* orderFlowData = (OrderFlowData*)sc.GetPersistentPointer(5);
    if (!orderFlowData) return;
    
    int index = sc.Index;
    if (index < 1) return;
    
    // Calculate current bar delta
    float currentDelta = sc.AskVolume[index] - sc.BidVolume[index];
    
    // Update cumulative delta
    float prevCumulativeDelta = sc.GetPersistentFloat(4);
    float newCumulativeDelta = prevCumulativeDelta + currentDelta;
    sc.SetPersistentFloat(4, newCumulativeDelta);
    
    // Store in subgraph for visualization
    sc.Subgraph[0][index] = newCumulativeDelta;
    
    // Calculate delta moving average
    sc.SimpleMovAvg(sc.Subgraph[0], sc.Subgraph[1], sc.Input[71].GetInt());
    
    // Calculate volume imbalance
    float totalVolume = sc.AskVolume[index] + sc.BidVolume[index];
    if (totalVolume > 0)
    {
        orderFlowData->volumeImbalance = std::abs(sc.AskVolume[index] - sc.BidVolume[index]) / totalVolume;
    }
    
    // Calculate absorption strength
    float priceRange = sc.High[index] - sc.Low[index];
    if (priceRange > 0 && totalVolume > 0)
    {
        orderFlowData->absorptionStrength = totalVolume / (priceRange / sc.TickSize);
    }
    
    // Update order flow data structure
    orderFlowData->cumulativeDelta = newCumulativeDelta;
    orderFlowData->deltaMA = sc.Subgraph[1][index];
}

void ProcessVolumeProfile(SCStudyInterfaceRef sc)
{
    std::vector<float>* hvnLevels = (std::vector<float>*)sc.GetPersistentPointer(1);
    std::vector<float>* lvnLevels = (std::vector<float>*)sc.GetPersistentPointer(2);
    OrderFlowData* orderFlowData = (OrderFlowData*)sc.GetPersistentPointer(5);
    
    if (!hvnLevels || !lvnLevels || !orderFlowData) return;
    
    int lookbackBars = sc.Input[83].GetInt();
    int startIndex = std::max(0, sc.Index - lookbackBars);
    
    // Clear previous levels
    hvnLevels->clear();
    lvnLevels->clear();
    orderFlowData->profileLevels.clear();
    
    // Build volume profile
    std::map<float, float> volumeAtPrice;
    
    for (int i = startIndex; i <= sc.Index; i++)
    {
        if (i < 0 || i >= sc.ArraySize) continue;
        
        float volume = sc.Volume[i];
        float high = sc.High[i];
        float low = sc.Low[i];
        
        // Distribute volume across price levels within the bar
        int numLevels = std::max(1, static_cast<int>((high - low) / sc.TickSize));
        float volumePerLevel = volume / numLevels;
        
        for (int level = 0; level < numLevels; level++)
        {
            float price = low + (level * sc.TickSize);
            volumeAtPrice[price] += volumePerLevel;
        }
    }
    
    if (volumeAtPrice.empty()) return;
    
    // Calculate average volume
    float totalVolume = 0;
    for (const auto& pair : volumeAtPrice)
    {
        totalVolume += pair.second;
    }
    float avgVolume = totalVolume / volumeAtPrice.size();
    
    // Identify HVN and LVN levels
    float hvnThreshold = avgVolume * sc.Input[81].GetFloat();
    float lvnThreshold = avgVolume * sc.Input[82].GetFloat();
    
    for (const auto& pair : volumeAtPrice)
    {
        float price = pair.first;
        float volume = pair.second;
        
        VolumeProfileLevel level;
        level.price = price;
        level.volume = volume;
        level.isHVN = (volume >= hvnThreshold);
        level.isLVN = (volume <= lvnThreshold);
        
        orderFlowData->profileLevels.push_back(level);
        
        if (level.isHVN)
        {
            hvnLevels->push_back(price);
            sc.Subgraph[6][sc.Index] = price;
        }
        
        if (level.isLVN)
        {
            lvnLevels->push_back(price);
            sc.Subgraph[7][sc.Index] = price;
        }
    }
}

void UpdateRiskMetrics(SCStudyInterfaceRef sc, RiskMetrics& metrics)
{
    s_SCPositionData positionData;
    sc.GetTradePosition(positionData);
    
    // Update daily P&L
    metrics.dailyPnL = positionData.DailyProfitLoss;
    sc.SetPersistentFloat(1, metrics.dailyPnL);
    
    // Calculate portfolio heat
    float accountBalance = positionData.AveragePrice * 100000; // Estimate account size
    if (accountBalance > 0)
    {
        metrics.portfolioHeat = std::abs(positionData.OpenProfitLoss) / accountBalance * 100.0f;
        sc.Subgraph[8][sc.Index] = metrics.portfolioHeat;
    }
    
    // Update win rate
    if (metrics.tradesTotal > 0)
    {
        metrics.winRate = static_cast<float>(metrics.tradesWin) / metrics.tradesTotal * 100.0f;
    }
    
    // Update profit factor
    if (metrics.largestLoss != 0)
    {
        metrics.profitFactor = std::abs(metrics.largestWin / metrics.largestLoss);
    }
    
    // Track maximum drawdown
    static float peakBalance = accountBalance;
    if (accountBalance > peakBalance)
    {
        peakBalance = accountBalance;
    }
    
    float currentDrawdown = (peakBalance - accountBalance) / peakBalance * 100.0f;
    if (currentDrawdown > metrics.maxDrawdown)
    {
        metrics.maxDrawdown = currentDrawdown;
    }
}

float CalculatePositionSize(SCStudyInterfaceRef sc, const TradeSignal& signal, const RiskMetrics& metrics)
{
    float riskPerTrade = sc.Input[14].GetFloat() / 100.0f; // Convert percentage to decimal
    float baseQuantity = sc.Input[2].GetInt();
    
    // Calculate risk amount in dollars
    s_SCPositionData positionData;
    sc.GetTradePosition(positionData);
    float accountBalance = positionData.AveragePrice * 100000; // Estimate account size
    
    float riskAmount = accountBalance * riskPerTrade;
    
    // Calculate position size based on stop loss distance
    float stopDistance = std::abs(signal.entryPrice - signal.stopLoss);
    if (stopDistance <= 0) return baseQuantity;
    
    float pointValue = sc.Input[94].GetFloat();
    float calculatedSize = riskAmount / (stopDistance * pointValue);
    
    // Apply confidence scaling
    calculatedSize *= signal.confidence;
    
    // Ensure minimum and maximum limits
    calculatedSize = std::max(1.0f, calculatedSize);
    calculatedSize = std::min(static_cast<float>(baseQuantity * 3), calculatedSize);
    
    return calculatedSize;
}

bool ValidateSignal(SCStudyInterfaceRef sc, const TradeSignal& signal)
{
    // Basic validation checks
    if (signal.direction == 0) return false;
    if (signal.confidence < 0.5f) return false;
    if (signal.entryPrice <= 0) return false;
    if (signal.stopLoss <= 0) return false;
    if (signal.target <= 0) return false;
    
    // Check if stop loss is in correct direction
    if (signal.direction == 1 && signal.stopLoss >= signal.entryPrice) return false;
    if (signal.direction == -1 && signal.stopLoss <= signal.entryPrice) return false;
    
    // Check if target is in correct direction
    if (signal.direction == 1 && signal.target <= signal.entryPrice) return false;
    if (signal.direction == -1 && signal.target >= signal.entryPrice) return false;
    
    // Check risk-reward ratio (minimum 1:1.5)
    float risk = std::abs(signal.entryPrice - signal.stopLoss);
    float reward = std::abs(signal.target - signal.entryPrice);
    if (reward / risk < 1.5f) return false;
    
    // Check portfolio heat limits
    RiskMetrics* riskMetrics = (RiskMetrics*)sc.GetPersistentPointer(3);
    if (riskMetrics && riskMetrics->portfolioHeat > sc.Input[13].GetFloat()) return false;
    
    return true;
}

void LogTrade(SCStudyInterfaceRef sc, const TradeSignal& signal, const std::string& action)
{
    if (!sc.Input[4].GetYesNo()) return; // Detailed logging disabled
    
    SCString logMsg;
    logMsg.Format("%s - %s: %s | Entry: %.2f | Stop: %.2f | Target: %.2f | Confidence: %.2f | Reason: %s",
                  action.c_str(),
                  signal.strategy.c_str(),
                  (signal.direction == 1) ? "LONG" : "SHORT",
                  signal.entryPrice,
                  signal.stopLoss,
                  signal.target,
                  signal.confidence,
                  signal.reason.c_str());
    
    sc.AddMessageToLog(logMsg, 0);
}

bool IsWithinTradingHours(SCStudyInterfaceRef sc)
{
    SCDateTime currentTime = sc.BaseDateTimeIn[sc.Index];
    SCDateTime tradingStart = sc.Input[21].GetTime();
    SCDateTime tradingEnd = sc.Input[22].GetTime();
    
    // Check if within trading hours
    bool withinTradingHours = (currentTime.GetTime() >= tradingStart.GetTime() && 
                              currentTime.GetTime() <= tradingEnd.GetTime());
    
    if (!withinTradingHours || !sc.Input[1].GetYesNo()) return false;
    
    // Avoid trading near market open/close
    SCDateTime marketOpen = HMS_TIME(9, 30, 0);
    SCDateTime marketClose = HMS_TIME(16, 0, 0);
    
    bool avoidOpenTime = (currentTime.GetTime() < (marketOpen.GetTime() + sc.Input[24].GetInt() * 60));
    bool avoidCloseTime = (currentTime.GetTime() > (marketClose.GetTime() - sc.Input[25].GetInt() * 60));
    
    return !(avoidOpenTime || avoidCloseTime);
}

float CalculateVolatility(SCStudyInterfaceRef sc, int lookback)
{
    if (sc.Index < lookback) return 0.0f;
    
    std::vector<float> returns;
    for (int i = sc.Index - lookback + 1; i <= sc.Index; i++)
    {
        if (i > 0 && sc.Close[i-1] > 0)
        {
            float return_val = (sc.Close[i] - sc.Close[i-1]) / sc.Close[i-1];
            returns.push_back(return_val);
        }
    }
    
    if (returns.empty()) return 0.0f;
    
    // Calculate standard deviation
    float mean = std::accumulate(returns.begin(), returns.end(), 0.0f) / returns.size();
    float variance = 0.0f;
    
    for (float ret : returns)
    {
        variance += (ret - mean) * (ret - mean);
    }
    variance /= returns.size();
    
    return std::sqrt(variance);
}

std::vector<float> FindSwingPoints(SCStudyInterfaceRef sc, int lookback, bool findHighs)
{
    std::vector<float> swingPoints;
    
    for (int i = lookback; i <= sc.Index - lookback; i++)
    {
        bool isSwingPoint = true;
        float currentValue = findHighs ? sc.High[i] : sc.Low[i];
        
        // Check if current point is higher/lower than surrounding points
        for (int j = i - lookback; j <= i + lookback; j++)
        {
            if (j == i) continue;
            
            float compareValue = findHighs ? sc.High[j] : sc.Low[j];
            
            if (findHighs && compareValue >= currentValue)
            {
                isSwingPoint = false;
                break;
            }
            else if (!findHighs && compareValue <= currentValue)
            {
                isSwingPoint = false;
                break;
            }
        }
        
        if (isSwingPoint)
        {
            swingPoints.push_back(currentValue);
        }
    }
    
    return swingPoints;
}

// ===============================================================================
// STRATEGY IMPLEMENTATION FUNCTIONS
// ===============================================================================

TradeSignal CheckLiquidityAbsorption(SCStudyInterfaceRef sc, int index)
{
    TradeSignal signal = {0, 0.0f, "Liquidity Absorption", 0.0f, 0.0f, 0.0f, ""};
    
    if (index < 5) return signal;

    int volumeThreshold = sc.Input[51].GetInt();
    int priceStallTicks = sc.Input[52].GetInt();
    int confirmationBars = sc.Input[53].GetInt();
    
    float priceStallRange = priceStallTicks * sc.TickSize;
    float currentHigh = sc.High[index];
    float currentLow = sc.Low[index];
    float currentClose = sc.Close[index];
    
    // Check for absorption at current low (potential long setup)
    if (sc.BidVolume[index] >= volumeThreshold)
    {
        float rangeTicks = (currentHigh - currentLow) / sc.TickSize;
        bool priceStalled = (rangeTicks <= priceStallTicks);
        bool closedOffLow = (currentClose > (currentLow + (currentHigh - currentLow) * 0.6f));
        
        if (priceStalled && closedOffLow)
        {
            // Check for confirmation in previous bars
            int confirmationCount = 0;
            for (int i = 1; i <= confirmationBars && (index - i) >= 0; i++)
            {
                if (sc.BidVolume[index - i] >= volumeThreshold * 0.7f)
                    confirmationCount++;
            }
            
            if (confirmationCount >= confirmationBars - 1)
            {
                signal.direction = 1; // Long
                signal.confidence = 0.75f + (static_cast<float>(confirmationCount) / confirmationBars * 0.25f);
                signal.entryPrice = currentClose + sc.TickSize;
                signal.stopLoss = currentLow - (2 * sc.TickSize);
                signal.target = currentClose + ((currentClose - signal.stopLoss) * 2.0f);
                signal.reason = "Absorption at Low - Volume: " + std::to_string(sc.BidVolume[index]);
                
                // Visualize the signal
                sc.Subgraph[2][index] = currentLow - sc.TickSize;
                sc.Subgraph[2].DataColor[index] = sc.Subgraph[2].PrimaryColor;
            }
        }
    }
    
    // Check for absorption at current high (potential short setup)
    if (sc.AskVolume[index] >= volumeThreshold)
    {
        float rangeTicks = (currentHigh - currentLow) / sc.TickSize;
        bool priceStalled = (rangeTicks <= priceStallTicks);
        bool closedOffHigh = (currentClose < (currentLow + (currentHigh - currentLow) * 0.4f));
        
        if (priceStalled && closedOffHigh)
        {
            // Check for confirmation in previous bars
            int confirmationCount = 0;
            for (int i = 1; i <= confirmationBars && (index - i) >= 0; i++)
            {
                if (sc.AskVolume[index - i] >= volumeThreshold * 0.7f)
                    confirmationCount++;
            }
            
            if (confirmationCount >= confirmationBars - 1)
            {
                signal.direction = -1; // Short
                signal.confidence = 0.75f + (static_cast<float>(confirmationCount) / confirmationBars * 0.25f);
                signal.entryPrice = currentClose - sc.TickSize;
                signal.stopLoss = currentHigh + (2 * sc.TickSize);
                signal.target = currentClose - ((signal.stopLoss - currentClose) * 2.0f);
                signal.reason = "Absorption at High - Volume: " + std::to_string(sc.AskVolume[index]);
                
                // Visualize the signal
                sc.Subgraph[2][index] = currentHigh + sc.TickSize;
                sc.Subgraph[2].DataColor[index] = sc.Subgraph[2].SecondaryColor;
            }
        }
    }
    
    return signal;
}

TradeSignal CheckIcebergDetection(SCStudyInterfaceRef sc, int index)
{
    TradeSignal signal = {0, 0.0f, "Iceberg Detection", 0.0f, 0.0f, 0.0f, ""};
    
    int minHitVolume = sc.Input[61].GetInt();
    int detectionBars = sc.Input[62].GetInt();
    int priceTolerance = sc.Input[63].GetInt();
    
    if (index < detectionBars) return signal;
    
    float tolerancePrice = priceTolerance * sc.TickSize;
    
    // Check for buy iceberg (repeated hits at bid level)
    float icebergLevel = sc.Low[index];
    int hitCount = 0;
    int totalVolume = 0;
    
    for (int i = 0; i < detectionBars; i++)
    {
        int barIndex = index - i;
        if (barIndex < 0) break;
        
        if (std::abs(sc.Low[barIndex] - icebergLevel) <= tolerancePrice)
        {
            if (sc.BidVolume[barIndex] >= minHitVolume)
            {
                hitCount++;
                totalVolume += sc.BidVolume[barIndex];
            }
        }
    }
    
    // Buy iceberg detected - potential long continuation
    if (hitCount >= detectionBars * 0.6f && totalVolume >= minHitVolume * detectionBars)
    {
        // Check if price is bouncing from iceberg level
        if (sc.Close[index] > icebergLevel + sc.TickSize)
        {
            signal.direction = 1; // Long
            signal.confidence = 0.6f + (static_cast<float>(hitCount) / detectionBars * 0.3f);
            signal.entryPrice = sc.Close[index] + sc.TickSize;
            signal.stopLoss = icebergLevel - (2 * sc.TickSize);
            signal.target = signal.entryPrice + ((signal.entryPrice - signal.stopLoss) * 1.5f);
            signal.reason = "Buy Iceberg Detected - Hits: " + std::to_string(hitCount) + 
                           " Volume: " + std::to_string(totalVolume);
            
            // Visualize the signal
            sc.Subgraph[3][index] = icebergLevel - (2 * sc.TickSize);
            sc.Subgraph[3].DataColor[index] = sc.Subgraph[3].PrimaryColor;
        }
    }
    
    // Check for sell iceberg (repeated hits at ask level)
    icebergLevel = sc.High[index];
    hitCount = 0;
    totalVolume = 0;
    
    for (int i = 0; i < detectionBars; i++)
    {
        int barIndex = index - i;
        if (barIndex < 0) break;
        
        if (std::abs(sc.High[barIndex] - icebergLevel) <= tolerancePrice)
        {
            if (sc.AskVolume[barIndex] >= minHitVolume)
            {
                hitCount++;
                totalVolume += sc.AskVolume[barIndex];
            }
        }
    }
    
    // Sell iceberg detected - potential short continuation
    if (hitCount >= detectionBars * 0.6f && totalVolume >= minHitVolume * detectionBars)
    {
        // Check if price is rejecting from iceberg level
        if (sc.Close[index] < icebergLevel - sc.TickSize)
        {
            signal.direction = -1; // Short
            signal.confidence = 0.6f + (static_cast<float>(hitCount) / detectionBars * 0.3f);
            signal.entryPrice = sc.Close[index] - sc.TickSize;
            signal.stopLoss = icebergLevel + (2 * sc.TickSize);
            signal.target = signal.entryPrice - ((signal.stopLoss - signal.entryPrice) * 1.5f);
            signal.reason = "Sell Iceberg Detected - Hits: " + std::to_string(hitCount) + 
                           " Volume: " + std::to_string(totalVolume);
            
            // Visualize the signal
            sc.Subgraph[3][index] = icebergLevel + (2 * sc.TickSize);
            sc.Subgraph[3].DataColor[index] = sc.Subgraph[3].SecondaryColor;
        }
    }
    
    return signal;
}

TradeSignal CheckDeltaDivergence(SCStudyInterfaceRef sc, int index)
{
    TradeSignal signal = {0, 0.0f, "Delta Divergence", 0.0f, 0.0f, 0.0f, ""};
    
    int lookbackPeriod = sc.Input[72].GetInt();
    float exhaustionThreshold = sc.Input[73].GetFloat();
    
    if (index < lookbackPeriod + 5) return signal;
    
    // Get cumulative delta from persistent storage
    float cumulativeDelta = sc.GetPersistentFloat(4);
    
    // Find recent swing high/low in price
    int priceHighIndex = sc.GetIndexOfHighestValue(sc.High, index - lookbackPeriod, index - 1);
    int priceLowIndex = sc.GetIndexOfLowestValue(sc.Low, index - lookbackPeriod, index - 1);
    
    // Check for bearish divergence (price higher high, delta lower high)
    if (priceHighIndex != -1 && sc.High[index] > sc.High[priceHighIndex])
    {
        if (sc.Subgraph[0][index] < sc.Subgraph[0][priceHighIndex])
        {
            float divergenceStrength = (sc.High[index] - sc.High[priceHighIndex]) / sc.TickSize;
            float deltaWeakness = (sc.Subgraph[0][priceHighIndex] - sc.Subgraph[0][index]) / 
                                std::abs(sc.Subgraph[0][priceHighIndex]);
            
            if (divergenceStrength >= 3.0f && deltaWeakness >= 0.1f)
            {
                signal.direction = -1; // Short
                signal.confidence = 0.7f + std::min(0.25f, deltaWeakness);
                signal.entryPrice = sc.Close[index] - sc.TickSize;
                signal.stopLoss = sc.High[index] + (2 * sc.TickSize);
                signal.target = signal.entryPrice - ((signal.stopLoss - signal.entryPrice) * 2.0f);
                signal.reason = "Bearish Delta Divergence - Strength: " + std::to_string(divergenceStrength);
                
                // Visualize the signal
                sc.Subgraph[4][index] = sc.High[index] + (2 * sc.TickSize);
                sc.Subgraph[4].DataColor[index] = sc.Subgraph[4].SecondaryColor;
            }
        }
    }
    
    // Check for bullish divergence (price lower low, delta higher low)
    if (priceLowIndex != -1 && sc.Low[index] < sc.Low[priceLowIndex])
    {
        if (sc.Subgraph[0][index] > sc.Subgraph[0][priceLowIndex])
        {
            float divergenceStrength = (sc.Low[priceLowIndex] - sc.Low[index]) / sc.TickSize;
            float deltaStrength = (sc.Subgraph[0][index] - sc.Subgraph[0][priceLowIndex]) / 
                                std::abs(sc.Subgraph[0][priceLowIndex]);
            
            if (divergenceStrength >= 3.0f && deltaStrength >= 0.1f)
            {
                signal.direction = 1; // Long
                signal.confidence = 0.7f + std::min(0.25f, deltaStrength);
                signal.entryPrice = sc.Close[index] + sc.TickSize;
                signal.stopLoss = sc.Low[index] - (2 * sc.TickSize);
                signal.target = signal.entryPrice + ((signal.entryPrice - signal.stopLoss) * 2.0f);
                signal.reason = "Bullish Delta Divergence - Strength: " + std::to_string(divergenceStrength);
                
                // Visualize the signal
                sc.Subgraph[4][index] = sc.Low[index] - (2 * sc.TickSize);
                sc.Subgraph[4].DataColor[index] = sc.Subgraph[4].PrimaryColor;
            }
        }
    }
    
    return signal;
}

TradeSignal CheckVolumeImbalance(SCStudyInterfaceRef sc, int index)
{
    TradeSignal signal = {0, 0.0f, "Volume Imbalance", 0.0f, 0.0f, 0.0f, ""};
    
    if (index < 2) return signal;
    
    // Calculate volume imbalance ratio
    float totalVolume = sc.AskVolume[index] + sc.BidVolume[index];
    if (totalVolume == 0) return signal;
    
    float askRatio = sc.AskVolume[index] / totalVolume;
    float bidRatio = sc.BidVolume[index] / totalVolume;
    
    // Significant imbalance thresholds
    const float strongImbalanceThreshold = 0.75f;  // 75% or more on one side
    const float minVolume = 30; // Minimum volume to consider
    
    if (totalVolume < minVolume) return signal;
    
    // Check for bullish imbalance (more buying pressure)
    if (askRatio >= strongImbalanceThreshold)
    {
        // Confirm with price action - should be closing near high
        float closePosition = (sc.Close[index] - sc.Low[index]) / 
                             std::max(sc.TickSize, sc.High[index] - sc.Low[index]);
        
        if (closePosition >= 0.6f) // Closing in upper 40% of range
        {
            signal.direction = 1; // Long
            signal.confidence = 0.65f + (askRatio - strongImbalanceThreshold) * 1.4f;
            signal.entryPrice = sc.Close[index] + sc.TickSize;
            signal.stopLoss = sc.Low[index] - sc.TickSize;
            signal.target = signal.entryPrice + ((signal.entryPrice - signal.stopLoss) * 1.5f);
            signal.reason = "Bullish Volume Imbalance - Ask Ratio: " + 
                           std::to_string(static_cast<int>(askRatio * 100)) + "%";
            
            // Visualize the signal
            sc.Subgraph[4][index] = sc.Low[index] - sc.TickSize;
            sc.Subgraph[4].DataColor[index] = sc.Subgraph[4].PrimaryColor;
        }
    }
    
    // Check for bearish imbalance (more selling pressure)
    if (bidRatio >= strongImbalanceThreshold)
    {
        // Confirm with price action - should be closing near low
        float closePosition = (sc.Close[index] - sc.Low[index]) / 
                             std::max(sc.TickSize, sc.High[index] - sc.Low[index]);
        
        if (closePosition <= 0.4f) // Closing in lower 40% of range
        {
            signal.direction = -1; // Short
            signal.confidence = 0.65f + (bidRatio - strongImbalanceThreshold) * 1.4f;
            signal.entryPrice = sc.Close[index] - sc.TickSize;
            signal.stopLoss = sc.High[index] + sc.TickSize;
            signal.target = signal.entryPrice - ((signal.stopLoss - signal.entryPrice) * 1.5f);
            signal.reason = "Bearish Volume Imbalance - Bid Ratio: " + 
                           std::to_string(static_cast<int>(bidRatio * 100)) + "%";
            
            // Visualize the signal
            sc.Subgraph[4][index] = sc.High[index] + sc.TickSize;
            sc.Subgraph[4].DataColor[index] = sc.Subgraph[4].SecondaryColor;
        }
    }
    
    return signal;
}


TradeSignal CheckStopRunAnticipation(SCStudyInterfaceRef sc, int index)
{
    TradeSignal signal = {0, 0.0f, "Stop Run Anticipation", 0.0f, 0.0f, 0.0f, ""};
    
    if (index < 20) return signal;
    
    int lookback = 20;
    
    // Find recent swing highs and lows where stops might be clustered
    std::vector<float> swingHighs = FindSwingPoints(sc, 5, true);
    std::vector<float> swingLows = FindSwingPoints(sc, 5, false);
    
    float currentPrice = sc.Close[index];
    float currentHigh = sc.High[index];
    float currentLow = sc.Low[index];
    
    // Check for stop run above recent swing high (potential short setup)
    for (float swingHigh : swingHighs)
    {
        float distanceToSwing = std::abs(currentHigh - swingHigh);
        
        // If we've just cleared a swing high with volume
        if (currentHigh > swingHigh && distanceToSwing <= 3 * sc.TickSize)
        {
            // Check for high volume on the breakout bar
            float avgVolume = 0;
            for (int i = 1; i <= 10; i++)
            {
                if (index - i >= 0)
                    avgVolume += sc.Volume[index - i];
            }
            avgVolume /= 10;
            
            if (sc.Volume[index] > avgVolume * 1.5f)
            {
                // Check if price is failing to continue higher (potential trap)
                if (sc.Close[index] < swingHigh + (2 * sc.TickSize))
                {
                    signal.direction = -1; // Short (fade the breakout)
                    signal.confidence = 0.7f;
                    signal.entryPrice = sc.Close[index] - sc.TickSize;
                    signal.stopLoss = currentHigh + (2 * sc.TickSize);
                    signal.target = swingHigh - (3 * sc.TickSize);
                    signal.reason = "Stop Run Fade - Failed breakout above " + std::to_string(swingHigh);
                    
                    // Visualize the signal
                    sc.Subgraph[5][index] = currentHigh + (2 * sc.TickSize);
                    sc.Subgraph[5].DataColor[index] = sc.Subgraph[5].SecondaryColor;
                    break;
                }
                else
                {
                    // Genuine breakout - ride the momentum
                    signal.direction = 1; // Long (ride the breakout)
                    signal.confidence = 0.65f;
                    signal.entryPrice = sc.Close[index] + sc.TickSize;
                    signal.stopLoss = swingHigh - sc.TickSize;
                    signal.target = signal.entryPrice + ((signal.entryPrice - signal.stopLoss) * 2.0f);
                    signal.reason = "Stop Run Momentum - Breakout above " + std::to_string(swingHigh);
                    
                    // Visualize the signal
                    sc.Subgraph[5][index] = currentLow - (2 * sc.TickSize);
                    sc.Subgraph[5].DataColor[index] = sc.Subgraph[5].PrimaryColor;
                    break;
                }
            }
        }
    }
    
    // Check for stop run below recent swing low (potential long setup)
    if (signal.direction == 0) // Only if no signal found above
    {
        for (float swingLow : swingLows)
        {
            float distanceToSwing = std::abs(currentLow - swingLow);
            
            // If we've just cleared a swing low with volume
            if (currentLow < swingLow && distanceToSwing <= 3 * sc.TickSize)
            {
                // Check for high volume on the breakdown bar
                float avgVolume = 0;
                for (int i = 1; i <= 10; i++)
                {
                    if (index - i >= 0)
                        avgVolume += sc.Volume[index - i];
                }
                avgVolume /= 10;
                
                if (sc.Volume[index] > avgVolume * 1.5f)
                {
                    // Check if price is failing to continue lower (potential trap)
                    if (sc.Close[index] > swingLow - (2 * sc.TickSize))
                    {
                        signal.direction = 1; // Long (fade the breakdown)
                        signal.confidence = 0.7f;
                        signal.entryPrice = sc.Close[index] + sc.TickSize;
                        signal.stopLoss = currentLow - (2 * sc.TickSize);
                        signal.target = swingLow + (3 * sc.TickSize);
                        signal.reason = "Stop Run Fade - Failed breakdown below " + std::to_string(swingLow);
                        
                        // Visualize the signal
                        sc.Subgraph[5][index] = currentLow - (2 * sc.TickSize);
                        sc.Subgraph[5].DataColor[index] = sc.Subgraph[5].PrimaryColor;
                        break;
                    }
                    else
                    {
                        // Genuine breakdown - ride the momentum
                        signal.direction = -1; // Short (ride the breakdown)
                        signal.confidence = 0.65f;
                        signal.entryPrice = sc.Close[index] - sc.TickSize;
                        signal.stopLoss = swingLow + sc.TickSize;
                        signal.target = signal.entryPrice - ((signal.stopLoss - signal.entryPrice) * 2.0f);
                        signal.reason = "Stop Run Momentum - Breakdown below " + std::to_string(swingLow);
                        
                        // Visualize the signal
                        sc.Subgraph[5][index] = currentHigh + (2 * sc.TickSize);
                        sc.Subgraph[5].DataColor[index] = sc.Subgraph[5].SecondaryColor;
                        break;
                    }
                }
            }
        }
    }
    
    return signal;
}

TradeSignal CheckHVNRejection(SCStudyInterfaceRef sc, int index)
{
    TradeSignal signal = {0, 0.0f, "HVN Rejection", 0.0f, 0.0f, 0.0f, ""};
    
    std::vector<float>* hvnLevels = (std::vector<float>*)sc.GetPersistentPointer(1);
    if (!hvnLevels || hvnLevels->empty()) return signal;
    
    float currentPrice = sc.Close[index];
    float currentHigh = sc.High[index];
    float currentLow = sc.Low[index];
    int proximityTicks = sc.Input[84].GetInt();
    float proximityRange = proximityTicks * sc.TickSize;
    
    // Check for rejection from HVN levels
    for (float hvnLevel : *hvnLevels)
    {
        // Check if price approached the HVN level
        bool approachedFromBelow = (currentLow <= hvnLevel + proximityRange && 
                                   currentLow >= hvnLevel - proximityRange);
        bool approachedFromAbove = (currentHigh >= hvnLevel - proximityRange && 
                                   currentHigh <= hvnLevel + proximityRange);
        
        if (approachedFromBelow)
        {
            // Check for rejection (price failed to close above HVN)
            if (currentPrice < hvnLevel && sc.High[index] >= hvnLevel)
            {
                // Confirm with volume and price action
                float rejectionStrength = (hvnLevel - currentPrice) / sc.TickSize;
                
                if (rejectionStrength >= 2.0f)
                {
                    signal.direction = -1; // Short
                    signal.confidence = 0.65f + std::min(0.25f, rejectionStrength / 10.0f);
                    signal.entryPrice = currentPrice - sc.TickSize;
                    signal.stopLoss = hvnLevel + (2 * sc.TickSize);
                    signal.target = signal.entryPrice - ((signal.stopLoss - signal.entryPrice) * 1.5f);
                    signal.reason = "HVN Rejection from Above - Level: " + std::to_string(hvnLevel);
                    
                    // Visualize the signal
                    sc.Subgraph[6][index] = hvnLevel;
                    break;
                }
            }
        }
        
        if (approachedFromAbove)
        {
            // Check for rejection (price failed to close below HVN)
            if (currentPrice > hvnLevel && sc.Low[index] <= hvnLevel)
            {
                // Confirm with volume and price action
                float rejectionStrength = (currentPrice - hvnLevel) / sc.TickSize;
                
                if (rejectionStrength >= 2.0f)
                {
                    signal.direction = 1; // Long
                    signal.confidence = 0.65f + std::min(0.25f, rejectionStrength / 10.0f);
                    signal.entryPrice = currentPrice + sc.TickSize;
                    signal.stopLoss = hvnLevel - (2 * sc.TickSize);
                    signal.target = signal.entryPrice + ((signal.entryPrice - signal.stopLoss) * 1.5f);
                    signal.reason = "HVN Rejection from Below - Level: " + std::to_string(hvnLevel);
                    
                    // Visualize the signal
                    sc.Subgraph[6][index] = hvnLevel;
                    break;
                }
            }
        }
    }
    
    return signal;
}

TradeSignal CheckLVNBreakout(SCStudyInterfaceRef sc, int index)
{
    TradeSignal signal = {0, 0.0f, "LVN Breakout", 0.0f, 0.0f, 0.0f, ""};
    
    std::vector<float>* lvnLevels = (std::vector<float>*)sc.GetPersistentPointer(2);
    if (!lvnLevels || lvnLevels->empty()) return signal;
    
    float currentPrice = sc.Close[index];
    float currentHigh = sc.High[index];
    float currentLow = sc.Low[index];
    int proximityTicks = sc.Input[84].GetInt();
    float proximityRange = proximityTicks * sc.TickSize;
    
    // Calculate average volume for comparison
    float avgVolume = 0;
    int volumeBars = 10;
    for (int i = 1; i <= volumeBars && (index - i) >= 0; i++)
    {
        avgVolume += sc.Volume[index - i];
    }
    avgVolume /= volumeBars;
    
    // Check for breakout through LVN levels
    for (float lvnLevel : *lvnLevels)
    {
        // Check if price is breaking through LVN with momentum
        bool breakingUp = (sc.Low[index - 1] <= lvnLevel && currentHigh > lvnLevel + proximityRange);
        bool breakingDown = (sc.High[index - 1] >= lvnLevel && currentLow < lvnLevel - proximityRange);
        
        if (breakingUp && sc.Volume[index] > avgVolume * 1.2f)
        {
            // Upward breakout through LVN
            signal.direction = 1; // Long
            signal.confidence = 0.6f + std::min(0.3f, (sc.Volume[index] / avgVolume - 1.0f));
            signal.entryPrice = currentPrice + sc.TickSize;
            signal.stopLoss = lvnLevel - sc.TickSize;
            signal.target = signal.entryPrice + ((signal.entryPrice - signal.stopLoss) * 2.0f);
            signal.reason = "LVN Upward Breakout - Level: " + std::to_string(lvnLevel);
            
            // Visualize the signal
            sc.Subgraph[7][index] = lvnLevel;
            break;
        }
        
        if (breakingDown && sc.Volume[index] > avgVolume * 1.2f)
        {
            // Downward breakout through LVN
            signal.direction = -1; // Short
            signal.confidence = 0.6f + std::min(0.3f, (sc.Volume[index] / avgVolume - 1.0f));
            signal.entryPrice = currentPrice - sc.TickSize;
            signal.stopLoss = lvnLevel + sc.TickSize;
            signal.target = signal.entryPrice - ((signal.stopLoss - signal.entryPrice) * 2.0f);
            signal.reason = "LVN Downward Breakout - Level: " + std::to_string(lvnLevel);
            
            // Visualize the signal
            sc.Subgraph[7][index] = lvnLevel;
            break;
        }
    }
    
    return signal;
}

TradeSignal CheckMomentumBreakout(SCStudyInterfaceRef sc, int index)
{
    TradeSignal signal = {0, 0.0f, "Momentum Breakout", 0.0f, 0.0f, 0.0f, ""};
    
    int lookbackPeriod = sc.Input[92].GetInt();
    float volumeMultiplier = sc.Input[91].GetFloat();
    int confirmationPeriod = sc.Input[93].GetInt();
    
    if (index < lookbackPeriod + confirmationPeriod) return signal;
    
    // Find recent range high and low
    int rangeHighIndex = sc.GetIndexOfHighestValue(sc.High, index - lookbackPeriod, index - 1);
    int rangeLowIndex = sc.GetIndexOfLowestValue(sc.Low, index - lookbackPeriod, index - 1);
    
    if (rangeHighIndex == -1 || rangeLowIndex == -1) return signal;
    
    float rangeHigh = sc.High[rangeHighIndex];
    float rangeLow = sc.Low[rangeLowIndex];
    float currentPrice = sc.Close[index];
    float currentHigh = sc.High[index];
    float currentLow = sc.Low[index];
    
    // Calculate average volume
    float avgVolume = 0;
    for (int i = 1; i <= lookbackPeriod; i++)
    {
        if (index - i >= 0)
            avgVolume += sc.Volume[index - i];
    }
    avgVolume /= lookbackPeriod;
    
    // Check for upward momentum breakout
    if (currentHigh > rangeHigh && sc.Volume[index] >= avgVolume * volumeMultiplier)
    {
        // Confirm momentum with price closing in upper portion of bar
        float barRange = currentHigh - currentLow;
        float closePosition = (currentPrice - currentLow) / std::max(sc.TickSize, barRange);
        
        if (closePosition >= 0.7f) // Closing in upper 30% of bar
        {
            // Check for momentum continuation in recent bars
            int momentumBars = 0;
            for (int i = 1; i <= confirmationPeriod; i++)
            {
                if (index - i >= 0 && sc.Close[index - i] > sc.Open[index - i])
                    momentumBars++;
            }
            
            if (momentumBars >= confirmationPeriod * 0.6f)
            {
                signal.direction = 1; // Long
                signal.confidence = 0.6f + std::min(0.3f, (sc.Volume[index] / avgVolume - 1.0f));
                signal.entryPrice = currentPrice + sc.TickSize;
                signal.stopLoss = rangeLow - sc.TickSize;
                signal.target = signal.entryPrice + ((signal.entryPrice - signal.stopLoss) * 2.0f);
                signal.reason = "Upward Momentum Breakout - Range High: " + std::to_string(rangeHigh);
                
                // Visualize the signal
                sc.Subgraph[5][index] = rangeHigh;
                sc.Subgraph[5].DataColor[index] = sc.Subgraph[5].PrimaryColor;
            }
        }
    }
    
    // Check for downward momentum breakout
    if (currentLow < rangeLow && sc.Volume[index] >= avgVolume * volumeMultiplier)
    {
        // Confirm momentum with price closing in lower portion of bar
        float barRange = currentHigh - currentLow;
        float closePosition = (currentPrice - currentLow) / std::max(sc.TickSize, barRange);
        
        if (closePosition <= 0.3f) // Closing in lower 30% of bar
        {
            // Check for momentum continuation in recent bars
            int momentumBars = 0;
            for (int i = 1; i <= confirmationPeriod; i++)
            {
                if (index - i >= 0 && sc.Close[index - i] < sc.Open[index - i])
                    momentumBars++;
            }
            
            if (momentumBars >= confirmationPeriod * 0.6f)
            {
                signal.direction = -1; // Short
                signal.confidence = 0.6f + std::min(0.3f, (sc.Volume[index] / avgVolume - 1.0f));
                signal.entryPrice = currentPrice - sc.TickSize;
                signal.stopLoss = rangeHigh + sc.TickSize;
                signal.target = signal.entryPrice - ((signal.stopLoss - signal.entryPrice) * 2.0f);
                signal.reason = "Downward Momentum Breakout - Range Low: " + std::to_string(rangeLow);
                
                // Visualize the signal
                sc.Subgraph[5][index] = rangeLow;
                sc.Subgraph[5].DataColor[index] = sc.Subgraph[5].SecondaryColor;
            }
        }
 }
    
    return signal;
}

TradeSignal CheckCumulativeDeltaTrend(SCStudyInterfaceRef sc, int index)
{
    TradeSignal signal = {0, 0.0f, "Cumulative Delta Trend", 0.0f, 0.0f, 0.0f, ""};
    
    if (index < 20) return signal;
    
    // Get cumulative delta values
    float currentDelta = sc.Subgraph[0][index];
    float deltaMA = sc.Subgraph[1][index];
    float prevDelta = (index > 0) ? sc.Subgraph[0][index - 1] : 0.0f;
    float prevDeltaMA = (index > 0) ? sc.Subgraph[1][index - 1] : 0.0f;
    
    // Calculate delta trend strength
    float deltaTrend = currentDelta - prevDelta;
    float deltaMATrend = deltaMA - prevDeltaMA;
    
    // Check for strong delta trend alignment with price
    float priceChange = sc.Close[index] - sc.Close[index - 1];
    bool deltaAlignedWithPrice = (deltaTrend > 0 && priceChange > 0) || 
                                (deltaTrend < 0 && priceChange < 0);
    
    // Check for delta above/below moving average
    bool deltaAboveMA = currentDelta > deltaMA;
    bool deltaRising = deltaTrend > 0;
    bool deltaMARising = deltaMATrend > 0;
    
    // Bullish delta trend signal
    if (deltaAlignedWithPrice && deltaAboveMA && deltaRising && deltaMARising)
    {
        // Calculate trend strength
        float trendStrength = std::abs(deltaTrend) / std::max(1.0f, std::abs(currentDelta));
        
        if (trendStrength >= 0.05f) // 5% change in delta
        {
            signal.direction = 1; // Long
            signal.confidence = 0.6f + std::min(0.3f, trendStrength * 5.0f);
            signal.entryPrice = sc.Close[index] + sc.TickSize;
            signal.stopLoss = sc.Low[index] - (2 * sc.TickSize);
            signal.target = signal.entryPrice + ((signal.entryPrice - signal.stopLoss) * 1.5f);
            signal.reason = "Bullish Delta Trend - Strength: " + std::to_string(trendStrength);
        }
    }
    
    // Bearish delta trend signal
    if (deltaAlignedWithPrice && !deltaAboveMA && !deltaRising && !deltaMARising)
    {
        // Calculate trend strength
        float trendStrength = std::abs(deltaTrend) / std::max(1.0f, std::abs(currentDelta));
        
        if (trendStrength >= 0.05f) // 5% change in delta
        {
            signal.direction = -1; // Short
            signal.confidence = 0.6f + std::min(0.3f, trendStrength * 5.0f);
            signal.entryPrice = sc.Close[index] - sc.TickSize;
            signal.stopLoss = sc.High[index] + (2 * sc.TickSize);
            signal.target = signal.entryPrice - ((signal.stopLoss - signal.entryPrice) * 1.5f);
            signal.reason = "Bearish Delta Trend - Strength: " + std::to_string(trendStrength);
        }
    }
    
    return signal;
}

TradeSignal CheckLiquidityTraps(SCStudyInterfaceRef sc, int index)
{
    TradeSignal signal = {0, 0.0f, "Liquidity Traps", 0.0f, 0.0f, 0.0f, ""};
    
    if (index < 10) return signal;
    
    // Look for sudden appearance and disappearance of large orders
    // This is simulated since we don't have direct DOM access in historical data
    
    float currentHigh = sc.High[index];
    float currentLow = sc.Low[index];
    float currentClose = sc.Close[index];
    float prevHigh = sc.High[index - 1];
    float prevLow = sc.Low[index - 1];
    
    // Calculate average volume and range
    float avgVolume = 0;
    float avgRange = 0;
    int lookback = 10;
    
    for (int i = 1; i <= lookback; i++)
    {
        if (index - i >= 0)
        {
            avgVolume += sc.Volume[index - i];
            avgRange += (sc.High[index - i] - sc.Low[index - i]);
        }
    }
    avgVolume /= lookback;
    avgRange /= lookback;
    
    // Look for trap patterns
    // Pattern 1: High volume bar with small range followed by reversal
    float currentRange = currentHigh - currentLow;
    bool highVolumeSmallRange = (sc.Volume[index] > avgVolume * 2.0f && 
                                currentRange < avgRange * 0.7f);
    
    if (highVolumeSmallRange)
    {
        // Check for reversal in next few bars (simulated)
        // In real implementation, this would monitor DOM changes
        
        // Bullish trap (fake selling pressure)
        if (currentClose < (currentLow + currentRange * 0.3f))
        {
            signal.direction = 1; // Long (fade the selling)
            signal.confidence = 0.65f;
            signal.entryPrice = currentClose + sc.TickSize;
            signal.stopLoss = currentLow - sc.TickSize;
            signal.target = signal.entryPrice + ((signal.entryPrice - signal.stopLoss) * 2.0f);
            signal.reason = "Liquidity Trap - Fake Selling Pressure";
        }
        
        // Bearish trap (fake buying pressure)
        if (currentClose > (currentLow + currentRange * 0.7f))
        {
            signal.direction = -1; // Short (fade the buying)
            signal.confidence = 0.65f;
            signal.entryPrice = currentClose - sc.TickSize;
            signal.stopLoss = currentHigh + sc.TickSize;
            signal.target = signal.entryPrice - ((signal.stopLoss - signal.entryPrice) * 2.0f);
            signal.reason = "Liquidity Trap - Fake Buying Pressure";
        }
    }
    
    // Pattern 2: Price spikes with immediate reversal (stop hunting)
    bool priceSpike = false;
    float spikeLevel = 0;
    
    // Check for upward spike
    if (currentHigh > prevHigh + (3 * sc.TickSize) && 
        currentClose < currentHigh - (2 * sc.TickSize))
    {
        priceSpike = true;
        spikeLevel = currentHigh;
        
        signal.direction = -1; // Short (fade the spike)
        signal.confidence = 0.7f;
        signal.entryPrice = sc.Close[index] - sc.TickSize;
        signal.stopLoss = spikeLevel + sc.TickSize;
        signal.target = signal.entryPrice - ((signal.stopLoss - signal.entryPrice) * 1.5f);
        signal.reason = "Liquidity Trap - Upward Spike Fade";
    }
    
    // Check for downward spike
    if (currentLow < prevLow - (3 * sc.TickSize) && 
        currentClose > currentLow + (2 * sc.TickSize))
    {
        priceSpike = true;
        spikeLevel = currentLow;
        
        signal.direction = 1; // Long (fade the spike)
        signal.confidence = 0.7f;
        signal.entryPrice = sc.Close[index] + sc.TickSize;
        signal.stopLoss = spikeLevel - sc.TickSize;
        signal.target = signal.entryPrice + ((signal.entryPrice - signal.stopLoss) * 1.5f);
        signal.reason = "Liquidity Trap - Downward Spike Fade";
    }
    
    return signal;
}

