#include "storage/influx_writer.h"
#include <sstream>
#include <iostream>

InfluxWriter::InfluxWriter(const std::string& org,
                           const std::string& bucket,
                           const std::string& token,
                           const std::string& url)
    : org_(org), bucket_(bucket), token_(token), url_(url), running_(true) {

    // Start async writer thread
    writerThread_ = std::thread(&InfluxWriter::writerLoop, this);
}

InfluxWriter::~InfluxWriter() {
    running_ = false;
    if (writerThread_.joinable()) {
        writerThread_.join();
    }
}

void InfluxWriter::writeAlphaSignal(const std::string& symbol,
                                    double momentum,
                                    double meanRevZ,
                                    double rsi,
                                    double vbr,
                                    const std::string& signalType) const {
    std::ostringstream data;
    data << "alpha_signal,symbol=" << symbol
         << " momentum=" << momentum
         << ",meanRevZ=" << meanRevZ
         << ",rsi=" << rsi
         << ",vbr=" << vbr
         << ",signal_type=\"" << signalType << "\"";

    const_cast<InfluxWriter*>(this)->writeAsync(data.str());
}

void InfluxWriter::writeMicrostructureSignal(const std::string& symbol,
                                             double vpin,
                                             double toxicity,
                                             double lambda,
                                             double spread,
                                             long timestamp) const {
    std::ostringstream data;
    data << "microstructure,symbol=" << symbol
         << " vpin=" << vpin
         << ",toxicity=" << toxicity
         << ",lambda=" << lambda
         << ",spread=" << spread
         << " " << timestamp << "000000";

    const_cast<InfluxWriter*>(this)->writeAsync(data.str());
}

void InfluxWriter::writeOrderFlowSignal(const std::string& symbol,
                                        double ofi,
                                        double bidPressure,
                                        double askPressure,
                                        double volumeDelta,
                                        long timestamp) const {
    std::ostringstream data;
    data << "orderflow,symbol=" << symbol
         << " ofi=" << ofi
         << ",bid_pressure=" << bidPressure
         << ",ask_pressure=" << askPressure
         << ",volume_delta=" << volumeDelta
         << " " << timestamp << "000000";

    const_cast<InfluxWriter*>(this)->writeAsync(data.str());
}

void InfluxWriter::writeRegimeSignal(const std::string& symbol,
                                     const std::string& regime,
                                     double hurstExponent,
                                     double volatility,
                                     double trendStrength,
                                     long timestamp) const {
    std::ostringstream data;
    data << "regime,symbol=" << symbol << ",regime=" << regime
         << " hurst=" << hurstExponent
         << ",volatility=" << volatility
         << ",trend_strength=" << trendStrength
         << " " << timestamp << "000000";

    const_cast<InfluxWriter*>(this)->writeAsync(data.str());
}

void InfluxWriter::writeVWAP(const std::string& symbol,
                             double vwap,
                             double deviation,
                             long timestamp) const {

    std::ostringstream data;
    data << "vwap,symbol=" << symbol
         << " vwap=" << vwap
         << ",deviation=" << deviation
         << " " << timestamp << "000000";

    const_cast<InfluxWriter*>(this)->writeAsync(data.str());
}

void InfluxWriter::writeCandle(const std::string& symbol,
                               double open,
                               double high,
                               double low,
                               double close,
                               double volume,
                               long timestamp) const {

    std::ostringstream data;
    data << "candles,symbol=" << symbol
         << " open=" << open
         << ",high=" << high
         << ",low=" << low
         << ",close=" << close
         << ",volume=" << volume
         << " " << timestamp << "000000";

    const_cast<InfluxWriter*>(this)->writeAsync(data.str());
}

void InfluxWriter::writePriceTick(const std::string& symbol,
                                  double price,
                                  double volume,
                                  long timestamp) const {

    std::ostringstream data;
    data << "ticks,symbol=" << symbol
         << " price=" << price
         << ",volume=" << volume
         << " " << timestamp << "000000";

    const_cast<InfluxWriter*>(this)->writeAsync(data.str());
}

void InfluxWriter::writeAsync(const std::string& lineProtocol) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    writeQueue_.push(lineProtocol);
}

void InfluxWriter::writerLoop() {
    while (running_) {
        std::string data;

        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            if (!writeQueue_.empty()) {
                data = writeQueue_.front();
                writeQueue_.pop();
            }
        }

        if (!data.empty()) {
            writeToInflux(data);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void InfluxWriter::writeToInflux(const std::string& data) const {
    std::ostringstream cmd;
    cmd << "curl -s -XPOST '" << url_
        << "/api/v2/write?org=" << org_
        << "&bucket=" << bucket_
        << "&precision=ns' "
        << "--header 'Authorization: Token " << token_ << "' "
        << "--data-binary '" << data << "' > /dev/null 2>&1";

    int result = system(cmd.str().c_str());

    if (result != 0) {
        std::cerr << "[InfluxDB] Write failed for: "
                  << data.substr(0, 60) << "..." << std::endl;
    }
}

void InfluxWriter::flush() {
    while (true) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (writeQueue_.empty()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
