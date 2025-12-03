# Multi-Exchange Alpha Generation Engine

Grafana dashboard for live metrics: http://localhost:3000/public-dashboards/604df6e06d3b40309d261401a70a87cb
-----

## Key Features

  * **Real-Time Data Streaming:** Ingests live, tick-level market data from multiple major exchanges.
  * **Low-Latency Signal Processing:** Computes complex microstructure and regime-aware signals with sub-millisecond latency.
  * **Production Monitoring:** Visualizes all real-time data and signals using Grafana and InfluxDB.
  * **Rigorous Backtesting:** Includes a full historical replay framework with order-book aware execution, slippage, and commission modeling.
  * **Performance Engineered:** Core engine implemented in high-performance C++17.

-----

## Core Signal Implementation

The engine implements advanced, research-backed signals based on market microstructure and order flow.

### Market Microstructure Models

  * **VPIN (Volume-Synchronized Probability of Informed Trading):** Measures trade-induced flow toxicity.
  * **Hasbrouck Price Impact Models:** Quantifies the permanent price impact of trades.
  * **Lee–Ready Trade Classification:** Determines trade direction (buyer/seller initiated).
  * **Roll Implicit Bid–Ask Spread Estimator:** Provides an estimate of the trading cost.

### Order Flow and Imbalance

  * **Order Flow Imbalance (OFI) & CVD:** Quantifies immediate directional market pressure.
  * **Bid vs Ask Pressure:** Aggregated metrics for order book dynamics.
  * **Trade Aggression Metrics:** Analysis of order size and urgency.

### Regime-Aware Trading

  * **Classification:** Detects volatility and trend/mean-reversion regimes.
  * **Adaptation:** Applies adaptive signal weighting based on the detected market regime.

-----

## Technical Stack

| Component | Technology | Role |
| :--- | :--- | :--- |
| **Engine Core** | C++17, CMake | High-performance, low-latency computation. |
| **Data Storage** | InfluxDB 2.x | Time-series backend with nanosecond precision. |
| **Visualization** | Grafana | Real-time monitoring and analytics dashboard. |
| **Networking** | IXWebSocket | Handles live market data feeds. |
| **Deployment** | Docker & Docker Compose | Full system orchestration on AWS EC2. |
| **Testing** | GoogleTest | Unit and integration testing. |

### Supported Data Sources

The engine currently supports live feeds from:

  * Binance
  * Coinbase Advanced Trade
  * Polygon.io

-----

## Performance Summary

Metrics derived from live deployment and stress testing:

  * **Latency:** Sub-millisecond tick-to-signal latency.
  * **Throughput:** Sustained processing of 10,000+ ticks/sec.
  * **Reliability:** 99.9% live uptime during continuous operation.
  * **Quality:** 80%+ automated test coverage.

-----

## Setup and Building

### Prerequisites

  * C++17 compiler (GCC $\ge 9$, Clang $\ge 10$)
  * CMake
  * Docker & Docker Compose
  * InfluxDB 2.x

### Build Instructions

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Running the Engine

| Command | Description |
| :--- | :--- |
| `./alpha_engine live` | Starts the engine in live market data mode. |
| `./alpha_engine backtest` | Executes a full historical backtest. |
| `docker-compose up -d` | Deploys the full stack (Engine, InfluxDB, Grafana). |

### Grafana Dashboard

A production Grafana dashboard configuration is included for monitoring.

```bash
# Dashboard file location
grafana-dashboard.json
```

-----

## Research Foundations

The system is built on established quantitative finance literature:

  * **Flow Toxicity (VPIN):** Easley, López de Prado, O'Hara (2012)
  * **Microstructure & Price Impact:** Hasbrouck (1991); Lee & Ready (1991)
  * **Order Flow Impact:** Cont, Kukanov, Stoikov (2014)
  * **Momentum & Mean Reversion:** Jegadeesh & Titman (1993)

-----

## Project Structure

```
.
├── grafana-provisioning/    # Grafana configuration files
├── include/                 # Headers
├── src/                     # Core engine implementation
├── CMakeLists.txt           # Build configuration
├── docker-compose.yaml      # Full system orchestration
├── grafana-dashboard.json   # Production Grafana dashboard export
└── LICENSE
```

-----

## License and Contact

This project is released under the **MIT License**. See LICENSE for details.

For questions, feedback, or collaboration, please open a GitHub issue or reachout on [LinkedIn](https://www.linkedin.com/in/maitri-dodiya-a054012a4/). 
