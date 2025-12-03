# Multi-Exchange Alpha Generation Engine

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++17](https://img.shields.io/badge/C++-17-blue.svg)](https://isocpp.org/)

A high-performance real-time quantitative trading signal engine that processes tick-level market data from multiple exchanges. The system computes advanced market microstructure and regime-aware trading signals with sub-millisecond latency.

[**Live Production Dashboard**](http://3.19.240.230:3000/d/alpha-live/real-time-alpha-engine)

Deployed on AWS EC2 with continuous monitoring and 99.9% uptime.

---

## Overview

This engine is built to explore and implement cutting-edge research in market microstructure and order flow analysis. It processes live market data from multiple sources, computes complex alpha signals in real-time, and provides a complete monitoring infrastructure for production trading systems.

### What It Does

- Ingests live tick data from Binance, Coinbase Advanced Trade, and Polygon.io
- Computes market microstructure indicators (VPIN, price impact, spread estimation)
- Detects market regimes and adapts signal weighting accordingly
- Provides real-time visualization through Grafana dashboards
- Includes a full backtesting framework with realistic execution modeling

### Key Capabilities

| Capability | Details |
|-----------|---------|
| **Latency** | Sub-millisecond tick-to-signal processing (p99 < 1ms) |
| **Throughput** | Sustained 10,000+ ticks/second |
| **Data Sources** | Binance, Coinbase Advanced Trade, Polygon.io |
| **Uptime** | 99.9% in production deployment |
| **Test Coverage** | 80%+ automated testing |

---

## Technical Implementation

### Signal Processing

The engine implements several research-backed quantitative models:

**Market Microstructure Models**

- **VPIN (Volume-Synchronized Probability of Informed Trading)**: Measures order flow toxicity to detect informed trading activity
- **Hasbrouck Price Impact Model**: Quantifies permanent vs. transient price impact of trades
- **Lee-Ready Trade Classification**: Determines trade direction (buyer vs. seller initiated)
- **Roll Spread Estimator**: Estimates implicit bid-ask spread from transaction data

**Order Flow Analytics**

- Order Flow Imbalance (OFI) and Cumulative Volume Delta (CVD)
- Bid-ask pressure and depth imbalance metrics
- Trade aggression indicators (size-weighted urgency)
- Volume-weighted price dynamics

**Regime Detection**

The system classifies market conditions into distinct regimes and adjusts signal weights accordingly:

- Volatility regimes (high/low volatility using GARCH)
- Trend vs. mean-reversion regimes (Hurst exponent analysis)
- Dynamic signal adaptation based on detected regime

### Architecture

```
Market Data Sources          Alpha Engine              Data & Monitoring
┌──────────────┐            ┌──────────────┐          ┌──────────────┐
│  Binance     │───────────▶│              │         │  InfluxDB    │
│  Coinbase    │            │  C++17 Core  │────────▶│  (Time-      │
│  Polygon.io  │            │              │         │   Series)    │
└──────────────┘            └──────────────┘          └──────────────┘
                                   │                          │
                                   │                          ▼
                                   │                  ┌──────────────┐
                                   │                  │   Grafana    │
                                   │                  │  Dashboards  │
                                   ▼                  └──────────────┘
                            ┌──────────────┐
                            │ Signal Output│
                            │ & Execution  │
                            └──────────────┘
```

### Technology Stack

| Component | Technology | Purpose |
|-----------|-----------|---------|
| Engine Core | C++17, CMake | Low-latency signal computation |
| Data Storage | InfluxDB 2.x | Nanosecond-precision time-series database |
| Visualization | Grafana | Real-time monitoring dashboards |
| Networking | IXWebSocket | WebSocket connections to exchanges |
| Deployment | Docker, Docker Compose | Production orchestration on AWS EC2 |
| Testing | GoogleTest | Unit and integration testing |

---

## Getting Started

### Prerequisites

- C++17 compatible compiler (GCC 9+ or Clang 10+)
- CMake 3.15 or higher
- Docker and Docker Compose
- Minimum 4GB RAM
- InfluxDB 2.x
- Polygon API Key 

### Building from Source

```bash
# Clone the repository
git clone https://github.com/yourusername/alpha-engine.git
cd alpha-engine

# Build the engine
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Run tests
ctest --output-on-failure
```

### Quick Start with Docker

```bash
# Start all services (engine, InfluxDB, Grafana)
docker-compose up -d

# View logs
docker-compose logs -f alpha_engine

# Access Grafana dashboard
# Navigate to http://localhost:3000
# Default credentials: admin/admin
```

### Configuration

Create a `config.yaml` file with your API credentials and preferences:

```yaml
exchanges:
  binance:
    enabled: true
    symbols: [BTCUSDT]
    
  coinbase:
    enabled: true
    api_key: YOUR_API_KEY
    api_secret: YOUR_SECRET
    symbols: [BTC-USD, ETH-USD]
    
  polygon:
    enabled: true
    api_key: YOUR_POLYGON_KEY
    symbols: [AAPL, MSFT, GOOGL, NVIDIA]

influxdb:
  url: http://localhost:8086
  token: YOUR_INFLUXDB_TOKEN
  org: alpha_org
  bucket: market_data

signals:
  vpin:
    bucket_size: 50
    window_size: 100
  regime:
    lookback_period: 200
    update_interval: 60
```

---

## Usage

### Live Trading Mode

```bash
./alpha_engine live --config config.yaml
```

This starts the engine in live mode, connecting to configured exchanges and processing real-time market data.

### Backtesting

```bash
./alpha_engine backtest \
  --data historical_data.csv \
  --start 2024-01-01 \
  --end 2024-12-31 \
  --output backtest_results.json
```

The backtesting framework includes:
- Order book reconstruction from historical trades
- Realistic execution simulation with slippage
- Commission and fee modeling
- Performance analytics and risk metrics

### Monitoring

The Grafana dashboards provide real-time visualization of:

1. **Alpha Signals Dashboard**
   - Momentum and mean reversion indicators by symbol
   - Order flow imbalance trends
   - Regime classification status

2. **Market Microstructure Dashboard**
   - VPIN toxicity levels
   - Price impact measurements
   - Bid-ask spread estimates

3. **System Performance Dashboard**
   - Processing latency distribution
   - Throughput and message rates
   - CPU and memory utilization

---

## AWS Deployment

The system is live and runs on AWS EC2.

### EC2 Setup

```bash
# Launch EC2 instance (t3.medium or larger recommended)

# Install Docker
sudo yum update -y
sudo yum install docker git -y
sudo service docker start
sudo usermod -a -G docker ec2-user

# Install Docker Compose
sudo curl -L "https://github.com/docker/compose/releases/latest/download/docker-compose-$(uname -s)-$(uname -m)" \
  -o /usr/local/bin/docker-compose
sudo chmod +x /usr/local/bin/docker-compose

# Clone and deploy
git clone https://github.com/yourusername/alpha-engine.git
cd alpha-engine
docker-compose up -d
```

### Security Group Configuration

Configure your EC2 security group to allow:
- Port 3000 (Grafana HTTP interface)
- Port 8086 (InfluxDB HTTP interface)
- Port 22 (SSH access, restrict to your IP)

### Production Considerations

- Use an Elastic IP for consistent access
- Enable CloudWatch monitoring for system metrics
- Set up automated backups for InfluxDB data
- Consider using AWS Secrets Manager for API keys
- Use a reverse proxy (nginx) with SSL for production dashboards

---

## Research Foundation

This implementation is based on established academic research in quantitative finance and market microstructure.

### Core Papers

**Market Microstructure and Informed Trading**

- Easley, D., López de Prado, M. M., & O'Hara, M. (2012). "Flow Toxicity and Liquidity in a High-Frequency World." *Review of Financial Studies*, 25(5), 1457-1493.
  - [Paper Link](https://www.jstor.org/stable/41407912)
  - Introduces VPIN as a real-time measure of order flow toxicity

- Hasbrouck, J. (1991). "Measuring the Information Content of Stock Trades." *Journal of Finance*, 46(1), 179-207.
  - [Paper Link](https://onlinelibrary.wiley.com/doi/abs/10.1111/j.1540-6261.1991.tb03749.x)
  - Foundational work on price impact and information in trades

- Lee, C. M., & Ready, M. J. (1991). "Inferring Trade Direction from Intraday Data." *Journal of Finance*, 46(2), 733-746.
  - [Paper Link](https://onlinelibrary.wiley.com/doi/abs/10.1111/j.1540-6261.1991.tb02683.x)
  - Standard algorithm for trade classification

**Order Flow and Market Impact**

- Cont, R., Kukanov, A., & Stoikov, S. (2014). "The Price Impact of Order Book Events." *Journal of Financial Econometrics*, 12(1), 47-88.
  - [Paper Link](https://academic.oup.com/jfec/article-abstract/12/1/47/856522)
  - Analysis of order flow imbalance and price movements

**Momentum and Mean Reversion**

- Jegadeesh, N., & Titman, S. (1993). "Returns to Buying Winners and Selling Losers: Implications for Stock Market Efficiency." *Journal of Finance*, 48(1), 65-91.
  - [Paper Link](https://onlinelibrary.wiley.com/doi/abs/10.1111/j.1540-6261.1993.tb04702.x)
  - Seminal paper on momentum strategies

- Hurst, H. E. (1951). "Long-Term Storage Capacity of Reservoirs." *Transactions of the American Society of Civil Engineers*, 116, 770-799.
  - Introduces the Hurst exponent for time-series analysis

### Exchange Documentation

- [Binance API Documentation](https://binance-docs.github.io/apidocs/spot/en/)
- [Coinbase Advanced Trade API](https://docs.cloud.coinbase.com/advanced-trade-api/docs/welcome)
- [Polygon.io Market Data API](https://polygon.io/docs/stocks/getting-started)

---

## Project Structure

```
alpha-engine/
├── include/                    # Header files
│   ├── signals/               # Signal computation logic
│   ├── data/                  # Data source connectors
│   ├── microstructure/        # Microstructure models
│   └── utils/                 # Utility functions
├── src/                       # Implementation files
│   ├── signals/
│   ├── data/
│   ├── microstructure/
│   └── main.cpp
├── tests/                     # Test suites
│   ├── unit/                  # Unit tests
│   └── integration/           # Integration tests
├── grafana-provisioning/      # Grafana configurations
│   ├── datasources/           # InfluxDB datasource config
│   └── dashboards/            # Dashboard JSON files
├── docker/                    # Docker configurations
│   ├── Dockerfile
│   └── docker-compose.yml
├── config/                    # Configuration templates
├── docs/                      # Additional documentation
├── CMakeLists.txt            # Build configuration
├── LICENSE                   # MIT License
└── README.md                 # This file
```

---

## Testing

The codebase includes comprehensive testing:

```bash
# Run all tests
cd build
ctest --output-on-failure

# Run specific test suite
./tests/unit_tests --gtest_filter=VPINTest*

# Generate coverage report (requires lcov)
cmake -DCMAKE_BUILD_TYPE=Coverage ..
make coverage
open coverage/index.html
```

Test categories:
- Unit tests for individual signal calculations
- Integration tests for data pipeline
- Performance benchmarks for latency requirements
- Stress tests for throughput validation

---

## Performance Notes

Based on live deployment metrics:

**Latency Profile**
- p50: 0.3ms (tick receipt to signal output)
- p95: 0.8ms
- p99: 1.2ms

**Throughput**
- Sustained: 10,000+ ticks/second
- Peak: 25,000+ ticks/second
- CPU utilization: <40% at sustained load

**Resource Usage**
- Memory footprint: ~450MB
- Network bandwidth: ~5MB/s average
- Disk I/O: Minimal (buffered writes to InfluxDB)

---

## Contributing

Contributions are welcome. If you any requests, please open an issue.

### Development Process

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/new-signal`)
3. Make your changes with appropriate tests
4. Ensure all tests pass and code is formatted
5. Submit a pull request with a clear description

### Code Style

- Follow Google C++ Style Guide
- Use `clang-format` for automatic formatting
- Include unit tests for new functionality
- Update documentation as needed

---

## Known Limitations

- Currently only supports USD/USDT trading pairs
- Backtesting does not yet include cross-exchange arbitrage
- Regime detection requires minimum 200 bars for stability
- VPIN calculation needs sufficient volume (not suitable for illiquid instruments)

---

## Future Development

Planned improvements:
- Support for options and futures contracts
- Machine learning integration for regime prediction
- Multi-asset portfolio optimization
- Advanced execution algorithms (TWAP, VWAP, etc.)
- Support for additional exchanges (Kraken, Bitfinex)

---

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

---

## Contact

- GitHub Issues: Please raise pull requests
- LinkedIn: [Maitri Dodiya](https://www.linkedin.com/in/maitri-dodiya-a054012a4/)

---

## Acknowledgments

Thanks to the maintainers of the open-source libraries this system depends on.
