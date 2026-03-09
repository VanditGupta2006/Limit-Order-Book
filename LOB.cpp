#include <bits/stdc++.h>
using namespace std;
// first iteration of LOB, where the orders are stored in vectors and sorted after each addition. This is not the most efficient way to implement an order book, but it serves as a simple starting point.
// Using LOBSTER data to test the implementation, which can be found at https://lobsterdata.com/info/data_files. The data files are in CSV format and contain historical order book data for various stocks.
class Order {
public:
    int orderId;
    bool isBuy; // true for Buy, false for Sell
    string orderType; // "L" or "M"
    double price;
    int quantity;
    long long timestamp; // seconds from midnight

    Order(int id, bool buy, string type, double p, int q , long long ts)
    {
        orderId = id;
        isBuy = buy;
        orderType = type;
        price = p;
        quantity = q;
        timestamp = ts;
    }
    string getReadableTime() const {
        //time given in seconds from midnight, convert to HH:MM:SS format
        long long hours = timestamp / 3600;
        long long minutes = (timestamp % 3600) / 60;
        long long seconds = timestamp % 60;
        char buffer[9];
        snprintf(buffer, sizeof(buffer), "%02lld:%02lld:%02lld", hours, minutes, seconds);
        return string(buffer);
    }
    };

class OrderBook {
public:
    vector<Order> buyOrders;
    vector<Order> sellOrders;
    // todo : remove orders who have 0 quantity inside sortOrders
    void sortOrders() {
        buyOrders.erase(remove_if(buyOrders.begin(), buyOrders.end(), [](const Order &order) { return order.quantity == 0; }), buyOrders.end());
        sellOrders.erase(remove_if(sellOrders.begin(), sellOrders.end(), [](const Order &order) { return order.quantity == 0; }), sellOrders.end());

        sort(buyOrders.begin(), buyOrders.end(), [](const Order &a, const Order &b) {
            if (a.price == b.price) return a.timestamp < b.timestamp;
            return a.price > b.price;
        });
        sort(sellOrders.begin(), sellOrders.end(), [](const Order &a, const Order &b) {
            if (a.price == b.price) return a.timestamp < b.timestamp;
            return a.price < b.price;
        });
    }

    void addLimitOrder(Order order) {
        if (order.isBuy) {
            for (auto &sellOrder : sellOrders) {
                if (order.quantity == 0) break;
                if (sellOrder.quantity == 0) continue;
                if (sellOrder.price <= order.price) {
                    int tradeQty = min(order.quantity, sellOrder.quantity);
                    double tradePrice = sellOrder.price;
                    cout << "Trade Executed: Buy " << order.orderId
                         << " with Sell " << sellOrder.orderId
                         << " Qty: " << tradeQty << " Price: " << tradePrice << "\n";
                    order.quantity -= tradeQty;
                    sellOrder.quantity -= tradeQty;
                }
            }
            if (order.quantity > 0) buyOrders.push_back(order);
        } else {
            for (auto &buyOrder : buyOrders) {
                if (order.quantity == 0) break;
                if (buyOrder.quantity == 0) continue;
                if (buyOrder.price >= order.price) {
                    int tradeQty = min(order.quantity, buyOrder.quantity);
                    double tradePrice = buyOrder.price;
                    cout << "Trade Executed: Sell " << order.orderId
                         << " with Buy " << buyOrder.orderId
                         << " Qty: " << tradeQty << " Price: " << tradePrice << "\n";
                    order.quantity -= tradeQty;
                    buyOrder.quantity -= tradeQty;
                }
            }
            if (order.quantity > 0) sellOrders.push_back(order);
        }
        sortOrders();
    }

    void addMarketOrder(Order order) {
        if (order.isBuy) {
            for (auto &sellOrder : sellOrders) {
                if (order.quantity == 0) break;
                if (sellOrder.quantity == 0) continue;
                int tradeQty = min(order.quantity, sellOrder.quantity);
                double tradePrice = sellOrder.price;
                cout << "Trade Executed: Market Buy " << order.orderId
                    << " with Sell " << sellOrder.orderId
                    << " Qty: " << tradeQty << " Price: " << tradePrice << "\n";
                order.quantity -= tradeQty;
                sellOrder.quantity -= tradeQty;
            }
        } else {
            for (auto &buyOrder : buyOrders) {
                if (order.quantity == 0) break;
                if (buyOrder.quantity == 0) continue;
                int tradeQty = min(order.quantity, buyOrder.quantity);
                double tradePrice = buyOrder.price;
                cout << "Trade Executed: Market Sell " << order.orderId
                    << " with Buy " << buyOrder.orderId
                    << " Qty: " << tradeQty << " Price: " << tradePrice << "\n";
                order.quantity -= tradeQty;
                buyOrder.quantity -= tradeQty;
            }
        }
        sortOrders();
    }

    void removeOrder(int orderId) {
        buyOrders.erase(remove_if(buyOrders.begin(), buyOrders.end(),
            [orderId](const Order &o){ return o.orderId == orderId; }), buyOrders.end());
        sellOrders.erase(remove_if(sellOrders.begin(), sellOrders.end(),
            [orderId](const Order &o){ return o.orderId == orderId; }), sellOrders.end());
        sortOrders();
    }
    void reduceOrder(int orderId, int reduceQty) {
        for (auto &o : buyOrders) {
            if (o.orderId == orderId) {
                o.quantity = max(0, o.quantity - reduceQty);
                return;
            }
        }
        for (auto &o : sellOrders) {
            if (o.orderId == orderId) {
                o.quantity = max(0, o.quantity - reduceQty);
                return;
            }
        }
        sortOrders();
    }
    void listOrders() {
        cout << "\n--- Order Book ---\n";
        cout << "Buy Orders:\n";
        for (const auto &o : buyOrders) {
            if (o.quantity > 0)
                cout << "ID: " << o.orderId 
                    << " Price: " << o.price 
                    << " Qty: " << o.quantity 
                    << " Time: " << o.getReadableTime() << "\n";
        }
        cout << "Sell Orders:\n";
        for (const auto &o : sellOrders) {
            if (o.quantity > 0)
                cout << "ID: " << o.orderId 
                    << " Price: " << o.price 
                    << " Qty: " << o.quantity 
                    << " Time: " << o.getReadableTime() << "\n";
        }
        cout << "------------------\n";
    }
    // add a function to just print the best bid and ask, along with best bid and ask quantities
    vector<double> printBestBidAsk() {
        vector<double> bestBidAsk(4, 0.0); // bestBid, bestBidQty, bestAsk, bestAskQty
        double bestBid = buyOrders.empty() ? 0.0 : buyOrders.front().price;
        int bestBidQty = buyOrders.empty() ? 0 : buyOrders.front().quantity;
        double bestAsk = sellOrders.empty() ? 0.0 : sellOrders.front().price;
        int bestAskQty = sellOrders.empty() ? 0 : sellOrders.front().quantity;

        bestBidAsk[0] = bestBid;
        bestBidAsk[1] = bestBidQty;
        bestBidAsk[2] = bestAsk;
        bestBidAsk[3] = bestAskQty;

        return bestBidAsk;
    }
};

int main() {
    string msgFilename = "AMZN_2012-06-21_34200000_57600000_message_1.csv"; // Replace with your message file 
    ifstream msgFile(msgFilename);
    
    if (!msgFile.is_open()) {
        cerr << "Error: Could not open LOBSTER file!\n";
        return 1;
    }

    // 1. Load message data (6 columns per row)
    vector<double> flatMessages;
    string line, token;
    cout << "Loading Message CSV into memory...\n";
    while (getline(msgFile, line)) {
        stringstream ss(line);
        while (getline(ss, token, ',')) {
            flatMessages.push_back(stod(token)); 
        }
    }
    msgFile.close();

    int totalEvents = flatMessages.size() / 6; 
    cout << "Loaded " << totalEvents << " events.\n";

    OrderBook ob;
    int mismatches = 0;

    // 3. Process and Verify
    cout << "Processing events and verifying book state...\n";
    for (int i = 0; i < totalEvents; i++) {
        int msgIdx = i * 6; 
        
        // --- Process Message ---
        double time      = flatMessages[msgIdx + 0]/10000.0; 
        int type         = static_cast<int>(flatMessages[msgIdx + 1]); 
        int id           = static_cast<int>(flatMessages[msgIdx + 2]); 
        int size         = static_cast<int>(flatMessages[msgIdx + 3]); 
        double price     = flatMessages[msgIdx + 4];
        int directionRaw = static_cast<int>(flatMessages[msgIdx + 5]); 
        
        bool isBuy = (directionRaw == 1); 

        if (type == 1) {
            Order newOrder(id, isBuy, "L", price, size, time);
            ob.addLimitOrder(newOrder);
        } 
        else if (type == 2 || type == 4 || type == 5) {
            ob.reduceOrder(id, size);
        } 
        else if (type == 3) {
            ob.removeOrder(id);
        }
        else if (type == 7) {
            continue;
        }
       // Creating a csv file storing the best bid and ask prices and sizes after processing each event, to compare with the orderbook csv file provided by LOBSTER
        ofstream outFile("my_orderbook.csv", ios::app);
        vector<double> myTop = ob.printBestBidAsk();
        outFile << time << "," << myTop[0] << "," << myTop[1] << "," << myTop[2] << "," << myTop[3] << "\n";
        outFile.close();
    }

    return 0;
}

