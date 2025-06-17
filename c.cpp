#include "sierrachart.h"

// Study ID
SCDLLName("Advanced Order Flow Trading Bot")

/*==========================================================================*/
// Study Configuration and Data Structures
/*==========================================================================*/

// Core configuration structure
struct BotConfiguration {
    // Strategy Enable/Disable Flags
    int EnableLiquidityAbsorption;
    int EnableIcebergDetection;
    int EnableLiquidityTraps;
    int EnableBreakoutConfirmation;
    int EnableStopRunAnticipation;
    int EnableVolumeImbalance;
    int EnableDeltaDivergence;
    int EnableHVNTrading;
    int EnableLVNTrading;
    int EnableCumulativeDelta;
    int RunAllStrategies;
    
    // Risk Management
    float MaxPositionSize;
    float MaxDailyLoss;
    float MaxDrawdown;
    int MaxConcurrentTrades;
    
    // Order Flow Parameters
    float AbsorptionThreshold;
    int IcebergMinSize;
    float DeltaDivergenceThreshold;
    int VolumeImbalanceRatio;
    
    // Timeframes
    int PrimaryTimeframe;
    int ConfirmationTimeframe;
};

// Order flow data structure
struct OrderFlowData {
    float BidVolume;
    float AskVolume;
    float Delta;
    float CumulativeDelta;
    float VolumeAtPrice[1000]; // Price levels
    int AbsorptionLevel;
    int IcebergDetected;
    float LiquidityImbalance;
    SCDateTime Timestamp;
};

// Trade management structure
struct TradeInfo {
    int TradeID;
    int StrategyType;
    float EntryPrice;
    float StopLoss;
    float Target1;
    float Target2;
    int Quantity;
    SCDateTime EntryTime;
    int IsActive;
    float UnrealizedPnL;
};

/*==========================================================================*/
// Main Study Function
/*==========================================================================*/
SCSFExport scsf_OrderFlowTradingBot(SCStudyInterfaceRef sc)
{
    // Configuration inputs
    SCInputRef Input_EnableLiquidityAbsorption = sc.Input[0];
    SCInputRef Input_EnableIcebergDetection = sc.Input[1];
    SCInputRef Input_EnableLiquidityTraps = sc.Input[2];
    SCInputRef Input_EnableBreakoutConfirmation = sc.Input[3];
    SCInputRef Input_EnableStopRunAnticipation = sc.Input[4];
    SCInputRef Input_EnableVolumeImbalance = sc.Input[5];
    SCInputRef Input_EnableDeltaDivergence = sc.Input[6];
    SCInputRef Input_EnableHVNTrading = sc.Input[7];
    SCInputRef Input_EnableLVNTrading = sc.Input[8];
    SCInputRef Input_EnableCumulativeDelta = sc.Input[9];
    SCInputRef Input_RunAllStrategies = sc.Input[10];
    
    // Risk management inputs
    SCInputRef Input_MaxPositionSize = sc.Input[20];
    SCInputRef Input_MaxDailyLoss = sc.Input[21];
    SCInputRef Input_MaxDrawdown = sc.Input[22];
    SCInputRef Input_MaxConcurrentTrades = sc.Input[23];
    
    // Order flow parameters
    SCInputRef Input_AbsorptionThreshold = sc.Input[30];
    SCInputRef Input_IcebergMinSize = sc.Input[31];
    SCInputRef Input_DeltaDivergenceThreshold = sc.Input[32];
    SCInputRef Input_VolumeImbalanceRatio = sc.Input[33];
    
    // Study configuration
    if (sc.SetDefaults)
    {
        sc.GraphName = "Advanced Order Flow Trading Bot";
        sc.StudyDescription = "Comprehensive order flow analysis and automated trading system";
        sc.AutoLoop = 1;
        sc.GraphRegion = 0;
        sc.FreeDLL = 1;
        sc.MaintainVolumeAtPriceData = 1;
        
        // Strategy enable/disable inputs
        Input_EnableLiquidityAbsorption.Name = "Enable Liquidity Absorption Strategy";
        Input_EnableLiquidityAbsorption.SetYesNo(1);
        
        Input_EnableIcebergDetection.Name = "Enable Iceberg Detection Strategy";
        Input_EnableIcebergDetection.SetYesNo(1);
        
        Input_EnableLiquidityTraps.Name = "Enable Liquidity Traps Strategy";
        Input_EnableLiquidityTraps.SetYesNo(1);
        
        Input_EnableBreakoutConfirmation.Name = "Enable Breakout Confirmation Strategy";
        Input_EnableBreakoutConfirmation.SetYesNo(1);
        
        Input_EnableStopRunAnticipation.Name = "Enable Stop Run Anticipation Strategy";
        Input_EnableStopRunAnticipation.SetYesNo(1);
        
        Input_EnableVolumeImbalance.Name = "Enable Volume Imbalance Strategy";
        Input_EnableVolumeImbalance.SetYesNo(1);
        
        Input_EnableDeltaDivergence.Name = "Enable Delta Divergence Strategy";
        Input_EnableDeltaDivergence.SetYesNo(1);
        
        Input_EnableHVNTrading.Name = "Enable HVN Trading Strategy";
        Input_EnableHVNTrading.SetYesNo(1);
        
        Input_EnableLVNTrading.Name = "Enable LVN Trading Strategy";
        Input_EnableLVNTrading.SetYesNo(1);
        
        Input_EnableCumulativeDelta.Name = "Enable Cumulative Delta Strategy";
        Input_EnableCumulativeDelta.SetYesNo(1);
        
        Input_RunAllStrategies.Name = "Run All Strategies (Override Individual Settings)";
        Input_RunAllStrategies.SetYesNo(0);
        
        // Risk management inputs
        Input_MaxPositionSize.Name = "Maximum Position Size";
        Input_MaxPositionSize.SetFloat(10.0f);
        
        Input_MaxDailyLoss.Name = "Maximum Daily Loss";
        Input_MaxDailyLoss.SetFloat(1000.0f);
        
        Input_MaxDrawdown.Name = "Maximum Drawdown";
        Input_MaxDrawdown.SetFloat(2000.0f);
        
        Input_MaxConcurrentTrades.Name = "Maximum Concurrent Trades";
        Input_MaxConcurrentTrades.SetInt(3);
        
        // Order flow parameters
        Input_AbsorptionThreshold.Name = "Absorption Threshold";
        Input_AbsorptionThreshold.SetFloat(0.7f);
        
        Input_IcebergMinSize.Name = "Iceberg Minimum Size";
        Input_IcebergMinSize.SetInt(100);
        
        Input_DeltaDivergenceThreshold.Name = "Delta Divergence Threshold";
        Input_DeltaDivergenceThreshold.SetFloat(0.3f);
        
        Input_VolumeImbalanceRatio.Name = "Volume Imbalance Ratio";
        Input_VolumeImbalanceRatio.SetInt(3);
        
        return;
    }
    
    // Persistent variables
    int& LastProcessedIndex = sc.GetPersistentInt(1);
    float& DailyPnL = sc.GetPersistentFloat(1);
    float& CumulativeDelta = sc.GetPersistentFloat(2);
    int& ActiveTrades = sc.GetPersistentInt(2);
    
    // Get current bar index
    int CurrentIndex = sc.Index;
    
    // Skip if not new bar or insufficient data
    if (CurrentIndex < 10 || CurrentIndex <= LastProcessedIndex)
        return;
    
    // Update last processed index
    LastProcessedIndex = CurrentIndex;
    
    // Check if trading is enabled
    if (!IsStrategyEnabled(sc, Input_RunAllStrategies.GetYesNo()))
        return;
    
    // Perform risk checks
    if (!PassesRiskChecks(sc, DailyPnL, ActiveTrades, Input_MaxDailyLoss.GetFloat(), 
                         Input_MaxConcurrentTrades.GetInt()))
        return;
    
    // Collect order flow data
    OrderFlowData orderFlow;
    CollectOrderFlowData(sc, CurrentIndex, orderFlow);
    
    // Update cumulative delta
    CumulativeDelta += orderFlow.Delta;
    
    // Execute enabled strategies
    ExecuteStrategies(sc, CurrentIndex, orderFlow, CumulativeDelta);
    
    // Update trade management
    ManageActiveTrades(sc, CurrentIndex);
    
    // Log system status
    LogSystemStatus(sc, CurrentIndex, orderFlow, DailyPnL, ActiveTrades);
}

/*==========================================================================*/
// Core Analysis Functions
/*==========================================================================*/

void CollectOrderFlowData(SCStudyInterfaceRef sc, int Index, OrderFlowData& data)
{
    // Get basic OHLC data
    SCFloatArrayRef Open = sc.BaseData[SC_OPEN];
    SCFloatArrayRef High = sc.BaseData[SC_HIGH];
    SCFloatArrayRef Low = sc.BaseData[SC_LOW];
    SCFloatArrayRef Close = sc.BaseData[SC_CLOSE];
    SCFloatArrayRef Volume = sc.BaseData[SC_VOLUME];
    
    // Calculate bid/ask volume approximation
    if (Close[Index] > Open[Index]) {
        data.AskVolume = Volume[Index] * 0.6f; // Bullish bar - more buying
        data.BidVolume = Volume[Index] * 0.4f;
    } else {
        data.AskVolume = Volume[Index] * 0.4f; // Bearish bar - more selling
        data.BidVolume = Volume[Index] * 0.6f;
    }
    
    // Calculate delta
    data.Delta = data.AskVolume - data.BidVolume;
    
    // Get time and sales data if available
    if (sc.VolumeAtPriceForBars != NULL && sc.VolumeAtPriceForBars->GetNumberOfBars() > Index) {
        const s_VolumeAtPriceV2* VolumeAtPrice = NULL;
        int NumVAPElements = sc.VolumeAtPriceForBars->GetVAPElementsForBar(Index, VolumeAtPrice);
        
        if (VolumeAtPrice != NULL) {
            // Process volume at price data
            ProcessVolumeAtPriceData(sc, VolumeAtPrice, NumVAPElements, data);
        }
    }
    
    // Detect absorption patterns
    DetectAbsorption(sc, Index, data);
    
    // Detect iceberg orders
    DetectIcebergOrders(sc, Index, data);
    
    // Calculate liquidity imbalance
    CalculateLiquidityImbalance(sc, Index, data);
    
    data.Timestamp = sc.BaseDateTimeIn[Index];
}

void ProcessVolumeAtPriceData(SCStudyInterfaceRef sc, const s_VolumeAtPriceV2* VolumeAtPrice, 
                             int NumElements, OrderFlowData& data)
{
    float TotalBidVolume = 0;
    float TotalAskVolume = 0;
    
    for (int i = 0; i < NumElements; i++) {
        float Price = VolumeAtPrice[i].PriceInTicks * sc.TickSize;
        unsigned int BidVol = VolumeAtPrice[i].BidVolume;
        unsigned int AskVol = VolumeAtPrice[i].AskVolume;
        
        TotalBidVolume += BidVol;
        TotalAskVolume += AskVol;
        
        // Store volume at specific price levels
        int PriceIndex = (int)((Price - sc.Low[sc.Index]) / sc.TickSize);
        if (PriceIndex >= 0 && PriceIndex < 1000) {
            data.VolumeAtPrice[PriceIndex] = BidVol + AskVol;
        }
    }
    
    data.BidVolume = TotalBidVolume;
    data.AskVolume = TotalAskVolume;
    data.Delta = TotalAskVolume - TotalBidVolume;
}

/*==========================================================================*/
// Strategy Implementation Functions
/*==========================================================================*/

void ExecuteStrategies(SCStudyInterfaceRef sc, int Index, const OrderFlowData& orderFlow, 
                      float CumulativeDelta)
{
    // Get strategy enable flags
    bool RunAll = sc.Input[10].GetYesNo();
    
    // Liquidity Absorption Strategy
    if (RunAll || sc.Input[0].GetYesNo()) {
        ExecuteLiquidityAbsorptionStrategy(sc, Index, orderFlow);
    }
    
    // Iceberg Detection Strategy
    if (RunAll || sc.Input[1].GetYesNo()) {
        ExecuteIcebergDetectionStrategy(sc, Index, orderFlow);
    }
    
    // Liquidity Traps Strategy
    if (RunAll || sc.Input[2].GetYesNo()) {
        ExecuteLiquidityTrapsStrategy(sc, Index, orderFlow);
    }
    
    // Breakout Confirmation Strategy
    if (RunAll || sc.Input[3].GetYesNo()) {
        ExecuteBreakoutConfirmationStrategy(sc, Index, orderFlow);
    }
    
    // Stop Run Anticipation Strategy
    if (RunAll || sc.Input[4].GetYesNo()) {
        ExecuteStopRunAnticipationStrategy(sc, Index, orderFlow);
    }
    
    // Volume Imbalance Strategy
    if (RunAll || sc.Input[5].GetYesNo()) {
        ExecuteVolumeImbalanceStrategy(sc, Index, orderFlow);
    }
    
    // Delta Divergence Strategy
    if (RunAll || sc.Input[6].GetYesNo()) {
        ExecuteDeltaDivergenceStrategy(sc, Index, orderFlow, CumulativeDelta);
    }
    
    // HVN Trading Strategy
    if (RunAll || sc.Input[7].GetYesNo()) {
        ExecuteHVNTradingStrategy(sc, Index, orderFlow);
    }
    
    // LVN Trading Strategy
    if (RunAll || sc.Input[8].GetYesNo()) {
        ExecuteLVNTradingStrategy(sc, Index, orderFlow);
    }
    
    // Cumulative Delta Strategy
    if (RunAll || sc.Input[9].GetYesNo()) {
        ExecuteCumulativeDeltaStrategy(sc, Index, orderFlow, CumulativeDelta);
    }
}

void ExecuteLiquidityAbsorptionStrategy(SCStudyInterfaceRef sc, int Index, const OrderFlowData& orderFlow)
{
    float AbsorptionThreshold = sc.Input[30].GetFloat();
    
    // Check for absorption at current price level
    if (orderFlow.AbsorptionLevel > 0) {
        float Price = sc.Close[Index];
        float Volume = sc.Volume[Index];
        
        // Calculate absorption ratio
        float AbsorptionRatio = orderFlow.Delta / Volume;
        
        if (abs(AbsorptionRatio) < AbsorptionThreshold && Volume > sc.Volume[Index-1] * 1.5f) {
            // Absorption detected - prepare reversal trade
            int Direction = (orderFlow.Delta > 0) ? -1 : 1; // Trade opposite to absorbed flow
            
            TradeInfo trade;
            trade.StrategyType = 1; // Absorption strategy
            trade.EntryPrice = Price;
            trade.Quantity = CalculatePositionSize(sc, Price);
            trade.StopLoss = Price - (Direction * 4 * sc.TickSize);
            trade.Target1 = Price + (Direction * 8 * sc.TickSize);
            trade.Target2 = Price + (Direction * 16 * sc.TickSize);
            
            ExecuteTrade(sc, trade, Direction);
            
            // Log the trade
            SCString logMsg;
            logMsg.Format("Absorption Trade: Dir=%d, Price=%.2f, Volume=%.0f, Delta=%.0f", 
                         Direction, Price, Volume, orderFlow.Delta);
            sc.AddMessageToLog(logMsg, 0);
        }
    }
}

void ExecuteIcebergDetectionStrategy(SCStudyInterfaceRef sc, int Index, const OrderFlowData& orderFlow)
{
    int IcebergMinSize = sc.Input[31].GetInt();
    
    if (orderFlow.IcebergDetected > 0) {
        float Price = sc.Close[Index];
        
        // Determine iceberg direction and trade with it
        int Direction = (orderFlow.IcebergDetected == 1) ? 1 : -1; // 1=buy iceberg, -1=sell iceberg
        
        TradeInfo trade;
        trade.StrategyType = 2; // Iceberg strategy
        trade.EntryPrice = Price;
        trade.Quantity = CalculatePositionSize(sc, Price);
        trade.StopLoss = Price - (Direction * 6 * sc.TickSize);
        trade.Target1 = Price + (Direction * 12 * sc.TickSize);
        trade.Target2 = Price + (Direction * 24 * sc.TickSize);
        
        ExecuteTrade(sc, trade, Direction);
        
        SCString logMsg;
        logMsg.Format("Iceberg Trade: Dir=%d, Price=%.2f, Type=%d", 
                     Direction, Price, orderFlow.IcebergDetected);
        sc.AddMessageToLog(logMsg, 0);
    }
}

void ExecuteVolumeImbalanceStrategy(SCStudyInterfaceRef sc, int Index, const OrderFlowData& orderFlow)
{
    int ImbalanceRatio = sc.Input[33].GetInt();
    
    // Check for significant volume imbalance
    float TotalVolume = orderFlow.BidVolume + orderFlow.AskVolume;
    if (TotalVolume > 0) {
        float BidRatio = orderFlow.BidVolume / TotalVolume;
        float AskRatio = orderFlow.AskVolume / TotalVolume;
        
        bool SignificantImbalance = false;
        int Direction = 0;
        
        if (AskRatio > 0.7f) { // Strong buying pressure
            SignificantImbalance = true;
            Direction = 1;
        } else if (BidRatio > 0.7f) { // Strong selling pressure
            SignificantImbalance = true;
            Direction = -1;
        }
        
        if (SignificantImbalance) {
            float Price = sc.Close[Index];
            
            TradeInfo trade;
            trade.StrategyType = 6; // Volume Imbalance strategy
            trade.EntryPrice = Price;
            trade.Quantity = CalculatePositionSize(sc, Price);
            trade.StopLoss = Price - (Direction * 5 * sc.TickSize);
            trade.Target1 = Price + (Direction * 10 * sc.TickSize);
            trade.Target2 = Price + (Direction * 20 * sc.TickSize);
            
            ExecuteTrade(sc, trade, Direction);
            
            SCString logMsg;
            logMsg.Format("Imbalance Trade: Dir=%d, Price=%.2f, BidRatio=%.2f, AskRatio=%.2f", 
                         Direction, Price, BidRatio, AskRatio);
            sc.AddMessageToLog(logMsg, 0);
        }
    }
}

void ExecuteDeltaDivergenceStrategy(SCStudyInterfaceRef sc, int Index, const OrderFlowData& orderFlow, 
                                   float CumulativeDelta)
{
    if (Index < 5) return; // Need sufficient history
    
    float DivergenceThreshold = sc.Input[32].GetFloat();
    
    // Check for price vs delta divergence
    float PriceChange = sc.Close[Index] - sc.Close[Index-3];
    float DeltaChange = orderFlow.Delta; // Current bar delta
    
    bool BullishDivergence = (PriceChange < 0 && DeltaChange > DivergenceThreshold);
    bool BearishDivergence = (PriceChange > 0 && DeltaChange < -DivergenceThreshold);
    
    if (BullishDivergence || BearishDivergence) {
        float Price = sc.Close[Index];
        int Direction = BullishDivergence ? 1 : -1;
        
        TradeInfo trade;
        trade.StrategyType = 7; // Delta Divergence strategy
        trade.EntryPrice = Price;
        trade.Quantity = CalculatePositionSize(sc, Price);
        trade.StopLoss = Price - (Direction * 6 * sc.TickSize);
        trade.Target1 = Price + (Direction * 12 * sc.TickSize);
        trade.Target2 = Price + (Direction * 24 * sc.TickSize);
        
        ExecuteTrade(sc, trade, Direction);
        
        SCString logMsg;
        logMsg.Format("Delta Divergence Trade: Dir=%d, Price=%.2f, PriceChg=%.2f, Delta=%.0f", 
                     Direction, Price, PriceChange, DeltaChange);
        sc.AddMessageToLog(logMsg, 0);
    }
}

/*==========================================================================*/
// Detection and Analysis Functions
/*==========================================================================*/

void DetectAbsorption(SCStudyInterfaceRef sc, int Index, OrderFlowData& data)
{
    if (Index < 3) return;
    
    float CurrentVolume = sc.Volume[Index];
    float AvgVolume = (sc.Volume[Index-1] + sc.Volume[Index-2] + sc.Volume[Index-3]) / 3.0f;
    float PriceRange = sc.High[Index] - sc.Low[Index];
    float TypicalRange = (sc.High[Index-1] - sc.Low[Index-1] + 
                         sc.High[Index-2] - sc.Low[Index-2] + 
                         sc.High[Index-3] - sc.Low[Index-3]) / 3.0f;
    
    // Absorption criteria: High volume, low price movement
    if (CurrentVolume > AvgVolume * 1.5f && PriceRange < TypicalRange * 0.7f) {
        data.AbsorptionLevel = 1;
    } else {
        data.AbsorptionLevel = 0;
    }
}

void DetectIcebergOrders(SCStudyInterfaceRef sc, int Index, OrderFlowData& data)
{
    // Simulated iceberg detection based on repeated volume at similar prices
    // In real implementation, this would analyze DOM data
    
    data.IcebergDetected = 0;
    
    if (Index < 5) return;
    
    float CurrentPrice = sc.Close[Index];
    float Volume = sc.Volume[Index];
    
    // Check for consistent volume at similar price levels
    int SimilarPriceBars = 0;
    float TotalVolume = 0;
    
    for (int i = 1; i <= 4; i++) {
        if (abs(sc.Close[Index-i] - CurrentPrice) <= 2 * sc.TickSize) {
            SimilarPriceBars++;
            TotalVolume += sc.Volume[Index-i];
        }
    }
    
    if (SimilarPriceBars >= 2 && TotalVolume > Volume * 3) {
        // Likely iceberg - determine direction based on close vs open
        if (sc.Close[Index] > sc.Open[Index]) {
            data.IcebergDetected = 1; // Buy iceberg
        } else {
            data.IcebergDetected = -1; // Sell iceberg
        }
    }
}

void CalculateLiquidityImbalance(SCStudyInterfaceRef sc, int Index, OrderFlowData& data)
{
    // Calculate imbalance ratio
    float TotalVolume = data.BidVolume + data.AskVolume;
    if (TotalVolume > 0) {
        data.LiquidityImbalance = (data.AskVolume - data.BidVolume) / TotalVolume;
    } else {
        data.LiquidityImbalance = 0;
    }
}

/*==========================================================================*/
// Risk Management and Utility Functions
/*==========================================================================*/

bool IsStrategyEnabled(SCStudyInterfaceRef sc, bool RunAll)
{
    // Check if any strategy is enabled or run all is selected
    return RunAll || 
           sc.Input[0].GetYesNo() || sc.Input[1].GetYesNo() || sc.Input[2].GetYesNo() ||
           sc.Input[3].GetYesNo() || sc.Input[4].GetYesNo() || sc.Input[5].GetYesNo() ||
           sc.Input[6].GetYesNo() || sc.Input[7].GetYesNo() || sc.Input[8].GetYesNo() ||
           sc.Input[9].GetYesNo();
}

bool PassesRiskChecks(SCStudyInterfaceRef sc, float DailyPnL, int ActiveTrades, 
                     float MaxDailyLoss, int MaxConcurrentTrades)
{
    // Check daily loss limit
    if (DailyPnL <= -MaxDailyLoss) {
        sc.AddMessageToLog("Daily loss limit reached. Trading disabled.", 1);
        return false;
    }
    
    // Check concurrent trades limit
    if (ActiveTrades >= MaxConcurrentTrades) {
        return false;
    }
    
    // Add additional risk checks as needed
    return true;
}

int CalculatePositionSize(SCStudyInterfaceRef sc, float Price)
{
    float MaxPositionSize = sc.Input[20].GetFloat();
    
    // Simple position sizing - can be enhanced with volatility-based sizing
    return (int)MaxPositionSize;
}

void ExecuteTrade(SCStudyInterfaceRef sc, const TradeInfo& trade, int Direction)
{
    // In a real implementation, this would interface with Sierra Chart's trading functions
    // For now, we'll log the trade details
    
    SCString tradeMsg;
    tradeMsg.Format("TRADE SIGNAL - Strategy: %d, Direction: %s, Entry: %.2f, Stop: %.2f, Target1: %.2f",
                   trade.StrategyType, 
                   (Direction > 0) ? "LONG" : "SHORT",
                   trade.EntryPrice,
                   trade.StopLoss,
                   trade.Target1);
    
    sc.AddMessageToLog(tradeMsg, 0);
    
    // Here you would add actual order submission code
    // Example: sc.BuyEntry(), sc.SellEntry(), etc.
}

void ManageActiveTrades(SCStudyInterfaceRef sc, int Index)
{
    // Trade management logic - monitor stops, targets, trailing stops, etc.
    // This would track open positions and manage exits
    
    float CurrentPrice = sc.Close[Index];
    
    // Example of basic trade management
    // In real implementation, you'd iterate through active trades
    // and check exit conditions
}

void LogSystemStatus(SCStudyInterfaceRef sc, int Index, const OrderFlowData& orderFlow, 
                    float DailyPnL, int ActiveTrades)
{
    // Periodic system status logging
    if (Index % 100 == 0) { // Log every 100 bars
        SCString statusMsg;
        statusMsg.Format("System Status - Bar: %d, Delta: %.0f, Daily P&L: %.2f, Active Trades: %d",
                        Index, orderFlow.Delta, DailyPnL, ActiveTrades);
        sc.AddMessageToLog(statusMsg, 0);
    }
}

/*==========================================================================*/
// Additional Strategy Implementations (Placeholder Functions)
/*==========================================================================*/

void ExecuteLiquidityTrapsStrategy(SCStudyInterfaceRef sc, int Index, const OrderFlowData& orderFlow)
{
    // Monitor for sudden liquidity appearance/disappearance
    if (Index < 5) return;
    
    float CurrentPrice = sc.Close[Index];
    float PreviousVolume = sc.Volume[Index-1];
    float CurrentVolume = sc.Volume[Index];
    
    // Detect rapid volume changes (potential trap setup)
    if (CurrentVolume > PreviousVolume * 3.0f) {
        // Large volume spike - potential trap setup
        
        // Check if price reverses quickly (trap execution)
        float PriceChange = abs(sc.Close[Index] - sc.Open[Index]);
        float TypicalRange = sc.TickSize * 4;
        
        if (PriceChange > TypicalRange) {
            // Sharp reversal detected - trade the trap
            int Direction = (sc.Close[Index] > sc.Open[Index]) ? 1 : -1;
            
            TradeInfo trade;
            trade.StrategyType = 3; // Liquidity Trap strategy
            trade.EntryPrice = CurrentPrice;
            trade.Quantity = CalculatePositionSize(sc, CurrentPrice);
            trade.StopLoss = CurrentPrice - (Direction * 3 * sc.TickSize);
            trade.Target1 = CurrentPrice + (Direction * 8 * sc.TickSize);
            trade.Target2 = CurrentPrice + (Direction * 16 * sc.TickSize);
            
            ExecuteTrade(sc, trade, Direction);
            
            SCString logMsg;
            logMsg.Format("Liquidity Trap Trade: Dir=%d, Price=%.2f, VolSpike=%.0f", 
                         Direction, CurrentPrice, CurrentVolume/PreviousVolume);
            sc.AddMessageToLog(logMsg, 0);
        }
    }
}

void ExecuteBreakoutConfirmationStrategy(SCStudyInterfaceRef sc, int Index, const OrderFlowData& orderFlow)
{
    if (Index < 20) return; // Need sufficient history for S/R levels
    
    // Calculate recent high/low levels (simple S/R)
    float RecentHigh = sc.High[Index];
    float RecentLow = sc.Low[Index];
    
    for (int i = 1; i <= 10; i++) {
        if (sc.High[Index-i] > RecentHigh) RecentHigh = sc.High[Index-i];
        if (sc.Low[Index-i] < RecentLow) RecentLow = sc.Low[Index-i];
    }
    
    float CurrentPrice = sc.Close[Index];
    float CurrentVolume = sc.Volume[Index];
    float AvgVolume = 0;
    
    // Calculate average volume
    for (int i = 1; i <= 10; i++) {
        AvgVolume += sc.Volume[Index-i];
    }
    AvgVolume /= 10.0f;
    
    // Check for breakout with volume confirmation
    bool BreakoutUp = (CurrentPrice > RecentHigh) && (CurrentVolume > AvgVolume * 1.5f);
    bool BreakoutDown = (CurrentPrice < RecentLow) && (CurrentVolume > AvgVolume * 1.5f);
    
    if (BreakoutUp || BreakoutDown) {
        // Confirm with order flow
        bool OrderFlowConfirms = false;
        int Direction = 0;
        
        if (BreakoutUp && orderFlow.Delta > 0) {
            OrderFlowConfirms = true;
            Direction = 1;
        } else if (BreakoutDown && orderFlow.Delta < 0) {
            OrderFlowConfirms = true;
            Direction = -1;
        }
        
        if (OrderFlowConfirms) {
            TradeInfo trade;
            trade.StrategyType = 4; // Breakout Confirmation strategy
            trade.EntryPrice = CurrentPrice;
            trade.Quantity = CalculatePositionSize(sc, CurrentPrice);
            trade.StopLoss = CurrentPrice - (Direction * 6 * sc.TickSize);
            trade.Target1 = CurrentPrice + (Direction * 15 * sc.TickSize);
            trade.Target2 = CurrentPrice + (Direction * 30 * sc.TickSize);
            
            ExecuteTrade(sc, trade, Direction);
            
            SCString logMsg;
            logMsg.Format("Breakout Trade: Dir=%d, Price=%.2f, Volume=%.0f, Delta=%.0f", 
                         Direction, CurrentPrice, CurrentVolume, orderFlow.Delta);
            sc.AddMessageToLog(logMsg, 0);
        }
    }
}

void ExecuteStopRunAnticipationStrategy(SCStudyInterfaceRef sc, int Index, const OrderFlowData& orderFlow)
{
    if (Index < 15) return;
    
    // Identify recent swing highs and lows (potential stop clusters)
    float SwingHigh = 0;
    float SwingLow = 999999;
    int SwingHighIndex = 0;
    int SwingLowIndex = 0;
    
    // Look back 10 bars for swing points
    for (int i = 1; i <= 10; i++) {
        if (sc.High[Index-i] > SwingHigh) {
            SwingHigh = sc.High[Index-i];
            SwingHighIndex = Index-i;
        }
        if (sc.Low[Index-i] < SwingLow) {
            SwingLow = sc.Low[Index-i];
            SwingLowIndex = Index-i;
        }
    }
    
    float CurrentPrice = sc.Close[Index];
    float CurrentVolume = sc.Volume[Index];
    
    // Calculate expected stop levels
    float BuyStopLevel = SwingHigh + (2 * sc.TickSize); // Stops above swing high
    float SellStopLevel = SwingLow - (2 * sc.TickSize);  // Stops below swing low
    
    // Check if price is approaching stop levels
    bool ApproachingBuyStops = (CurrentPrice >= SwingHigh - (3 * sc.TickSize)) && 
                               (CurrentPrice <= BuyStopLevel);
    bool ApproachingSellStops = (CurrentPrice <= SwingLow + (3 * sc.TickSize)) && 
                                (CurrentPrice >= SellStopLevel);
    
    if (ApproachingBuyStops || ApproachingSellStops) {
        // Check for volume acceleration (stop run execution)
        float AvgVolume = (sc.Volume[Index-1] + sc.Volume[Index-2] + sc.Volume[Index-3]) / 3.0f;
        
        if (CurrentVolume > AvgVolume * 2.0f) {
            int Direction = 0;
            bool TradeStopRun = false;
            
            if (ApproachingBuyStops && CurrentPrice > SwingHigh) {
                // Buy stops triggered - trade with momentum
                Direction = 1;
                TradeStopRun = true;
            } else if (ApproachingSellStops && CurrentPrice < SwingLow) {
                // Sell stops triggered - trade with momentum  
                Direction = -1;
                TradeStopRun = true;
            }
            
            if (TradeStopRun) {
                TradeInfo trade;
                trade.StrategyType = 5; // Stop Run strategy
                trade.EntryPrice = CurrentPrice;
                trade.Quantity = CalculatePositionSize(sc, CurrentPrice) * 0.8f; // Smaller size for aggressive strategy
                trade.StopLoss = CurrentPrice - (Direction * 4 * sc.TickSize);
                trade.Target1 = CurrentPrice + (Direction * 10 * sc.TickSize);
                trade.Target2 = CurrentPrice + (Direction * 20 * sc.TickSize);
                
                ExecuteTrade(sc, trade, Direction);
                
                SCString logMsg;
                logMsg.Format("Stop Run Trade: Dir=%d, Price=%.2f, SwingLevel=%.2f, Volume=%.0f", 
                             Direction, CurrentPrice, 
                             (Direction > 0) ? SwingHigh : SwingLow, CurrentVolume);
                sc.AddMessageToLog(logMsg, 0);
            }
        }
    }
}

void ExecuteHVNTradingStrategy(SCStudyInterfaceRef sc, int Index, const OrderFlowData& orderFlow)
{
    if (Index < 50) return; // Need sufficient data for volume profile
    
    // Build simple volume profile for recent bars
    const int LookbackBars = 50;
    const int PriceLevels = 100;
    float VolumeProfile[PriceLevels] = {0};
    
    // Find price range
    float HighestPrice = sc.High[Index];
    float LowestPrice = sc.Low[Index];
    
    for (int i = 1; i < LookbackBars && (Index-i) >= 0; i++) {
        if (sc.High[Index-i] > HighestPrice) HighestPrice = sc.High[Index-i];
        if (sc.Low[Index-i] < LowestPrice) LowestPrice = sc.Low[Index-i];
    }
    
    float PriceRange = HighestPrice - LowestPrice;
    if (PriceRange <= 0) return;
    
    float PriceStep = PriceRange / PriceLevels;
    
    // Build volume profile
    for (int i = 1; i < LookbackBars && (Index-i) >= 0; i++) {
        float BarPrice = (sc.High[Index-i] + sc.Low[Index-i] + sc.Close[Index-i]) / 3.0f;
        int PriceLevel = (int)((BarPrice - LowestPrice) / PriceStep);
        
        if (PriceLevel >= 0 && PriceLevel < PriceLevels) {
            VolumeProfile[PriceLevel] += sc.Volume[Index-i];
        }
    }
    
    // Find High Volume Nodes (HVN)
    float MaxVolume = 0;
    int HVNLevel = 0;
    
    for (int i = 0; i < PriceLevels; i++) {
        if (VolumeProfile[i] > MaxVolume) {
            MaxVolume = VolumeProfile[i];
            HVNLevel = i;
        }
    }
    
    // Calculate HVN price
    float HVNPrice = LowestPrice + (HVNLevel * PriceStep);
    float CurrentPrice = sc.Close[Index];
    
    // Check if price is near HVN
    float DistanceToHVN = abs(CurrentPrice - HVNPrice);
    
    if (DistanceToHVN <= 3 * sc.TickSize) {
        // Price at HVN - look for rejection or breakout
        float PriceChange = CurrentPrice - sc.Open[Index];
        
        // Rejection trade
        if (abs(PriceChange) > 2 * sc.TickSize) {
            int Direction = (PriceChange > 0) ? -1 : 1; // Fade the move
            
            TradeInfo trade;
            trade.StrategyType = 8; // HVN strategy
            trade.EntryPrice = CurrentPrice;
            trade.Quantity = CalculatePositionSize(sc, CurrentPrice);
            trade.StopLoss = CurrentPrice - (Direction * 4 * sc.TickSize);
            trade.Target1 = CurrentPrice + (Direction * 8 * sc.TickSize);
            trade.Target2 = CurrentPrice + (Direction * 16 * sc.TickSize);
            
            ExecuteTrade(sc, trade, Direction);
            
            SCString logMsg;
            logMsg.Format("HVN Rejection Trade: Dir=%d, Price=%.2f, HVN=%.2f, Volume=%.0f", 
                         Direction, CurrentPrice, HVNPrice, MaxVolume);
            sc.AddMessageToLog(logMsg, 0);
        }
    }
}

void ExecuteLVNTradingStrategy(SCStudyInterfaceRef sc, int Index, const OrderFlowData& orderFlow)
{
    // Implementation for Low Volume Node trading
    // This would identify LVNs and trade continuations/rejections
}

void ExecuteCumulativeDeltaStrategy(SCStudyInterfaceRef sc, int Index, const OrderFlowData& orderFlow, 
                                   float CumulativeDelta)
{
    // Implementation for cumulative delta trend analysis
    // This would use cumulative delta for trend confirmation and reversals
}