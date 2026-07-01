#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <queue>
#include <algorithm>
#include <iomanip>
#include <memory>
#include <chrono>
#include <limits>
#include <unordered_map>

#include "Node.h"
#include "Gate.h"
#include "Circuit.h"
#include "TimingAnalyzer.h"

using namespace std;

// ============================================================================
// NODE IMPLEMENTATION
// ============================================================================

Node::Node(const string& name, bool isInput, bool isOutput) 
    : name(name), isPrimaryInput(isInput), isPrimaryOutput(isOutput),
      arrivalTimeRise(0.0), arrivalTimeFall(0.0),
      requiredTimeRise(0.0), requiredTimeFall(0.0),
      slackRise(0.0), slackFall(0.0),
      slewRise(0.0), slewFall(0.0),
      capacitance(0.0), fanoutCount(0) {
}

double Node::getMaxArrivalTime() const {
    return max(arrivalTimeRise, arrivalTimeFall);
}

double Node::getMinRequiredTime() const {
    return min(requiredTimeRise, requiredTimeFall);
}

double Node::getWorstSlack() const {
    return min(slackRise, slackFall);
}

void Node::resetTiming() {
    arrivalTimeRise = 0.0;
    arrivalTimeFall = 0.0;
    requiredTimeRise = 0.0;
    requiredTimeFall = 0.0;
    slackRise = 0.0;
    slackFall = 0.0;
    slewRise = 0.0;
    slewFall = 0.0;
}

void Node::printTiming() const {
    cout << fixed << setprecision(3);
    cout << "Node: " << name << endl;
    cout << "  Arrival Time (Rise/Fall): " << arrivalTimeRise << " / " << arrivalTimeFall << " ns" << endl;
    cout << "  Required Time (Rise/Fall): " << requiredTimeRise << " / " << requiredTimeFall << " ns" << endl;
    cout << "  Slack (Rise/Fall): " << slackRise << " / " << slackFall << " ns" << endl;
    cout << "  Slew (Rise/Fall): " << slewRise << " / " << slewFall << " ns" << endl;
    cout << "  Capacitance: " << capacitance << " fF" << endl;
    cout << "  Fanout Count: " << fanoutCount << endl;
    cout << "  Worst Slack: " << getWorstSlack() << " ns" << endl;
}

// ============================================================================
// CIRCUIT IMPLEMENTATION
// ============================================================================

Circuit::Circuit() : clockPeriod(1.0) {
    
}

void Circuit::loadCircuit(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        throw runtime_error("Cannot open circuit file: " + filename);
    }

    string line;
    while (getline(file, line)) {
        
        if (line.empty() || line[0] == '#') continue;
        
        istringstream iss(line);
        string command;
        iss >> command;
        
        if (command == "CLOCK_PERIOD") {
            iss >> clockPeriod;
        }
        else if (command == "INPUT") {
            string inputName;
            while (iss >> inputName) {
                addNode(inputName, true, false);
                primaryInputs.push_back(inputName);
            }
        }
        else if (command == "OUTPUT") {
            string outputName;
            while (iss >> outputName) {
                addNode(outputName, false, true);
                primaryOutputs.push_back(outputName);
            }
        }
        else if (command == "GATE") {
            string gateType, gateName, outputName;
            vector<string> inputs;
            
            iss >> gateType >> gateName >> outputName;
            
            string input;
            while (iss >> input) {
                inputs.push_back(input);
            }
            
            addGate(gateType, gateName, inputs, outputName);
        }
    }
    
    file.close();
    
    if (!validateCircuit()) {
        throw runtime_error("Invalid circuit configuration");
    }
}

void Circuit::loadDelays(const string& filename) {
    ifstream file(filename);
    if (!file.is_open()) {
        throw runtime_error("Cannot open delay file: " + filename);
    }

    string line;
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        
        istringstream iss(line);
        string gateType;
        double delay;
        
        if (iss >> gateType >> delay) {
            gateDelays[gateType] = delay;
        }
    }
    
    file.close();
}

void Circuit::addNode(const string& name, bool isInput, bool isOutput) {
    if (nodes.find(name) == nodes.end()) {
        nodes[name] = make_shared<Node>(name, isInput, isOutput);
    }
}

void Circuit::addGate(const string& type, const string& name, 
                     const vector<string>& inputs, const string& output) {
    // Ensure all nodes exist
    for (const auto& input : inputs) {
        addNode(input);
    }
    addNode(output);
    
    // Create gate
    auto gate = GateFactory::createGate(type, name, inputs, output);
    if (gate) {
        gates.push_back(gate);
        
        // Connect nodes
        for (const auto& input : inputs) {
            nodes[input]->addFanout(gate);
        }
        nodes[output]->setFanin(gate);
    }
}

shared_ptr<Node> Circuit::getNode(const string& name) {
    auto it = nodes.find(name);
    return (it != nodes.end()) ? it->second : nullptr;
}

double Circuit::getGateDelay(const string& gateType) const {
    auto it = gateDelays.find(gateType);
    return (it != gateDelays.end()) ? it->second : 0.0;
}

void Circuit::printCircuit() const {
    cout << "\n=== Circuit Information ===" << endl;
    cout << "Clock Period: " << clockPeriod << " ns" << endl;
    
    cout << "\nPrimary Inputs: ";
    for (const auto& input : primaryInputs) {
        cout << input << " ";
    }
    cout << endl;
    
    cout << "Primary Outputs: ";
    for (const auto& output : primaryOutputs) {
        cout << output << " ";
    }
    cout << endl;
    
    cout << "\nGates:" << endl;
    for (const auto& gate : gates) {
        cout << "  " << gate->getType() << " " << gate->getName() 
                  << " -> " << gate->getOutput() << endl;
    }
    
    cout << "\nGate Delays:" << endl;
    for (const auto& delay : gateDelays) {
        cout << "  " << delay.first << ": " << delay.second << " ns" << endl;
    }
}

bool Circuit::validateCircuit() const {
    // Checking if all gates have valid inputs and output
    for (const auto& gate : gates) {
        for (const auto& input : gate->getInputs()) {
            if (nodes.find(input) == nodes.end()) {
                cerr << "Error: Gate " << gate->getName() 
                          << " references undefined input " << input << endl;
                return false;
            }
        }
        
        if (nodes.find(gate->getOutput()) == nodes.end()) {
            cerr << "Error: Gate " << gate->getName() 
                      << " references undefined output " << gate->getOutput() << endl;
            return false;
        }
    }
    
    return true;
}


// ============================================================================
// TIMING ANALYZER IMPLEMENTATION
// ============================================================================

TimingAnalyzer::TimingAnalyzer(Circuit& circuit) 
    : circuit(circuit), worstSlack(0.0), totalDelay(0.0) {
}

void TimingAnalyzer::analyze() {
    cout << "Starting Static Timing Analysis..." << endl;
 
    resetAnalysis();
 
    // Step 1: Forward pass — O(V + E)
    cout << "Calculating arrival times..." << endl;
    calculateArrivalTimes();
 
    // Step 2: Backward pass — O(V + E)
    cout << "Calculating required times..." << endl;
    calculateRequiredTimes();
 
    // Step 3: Slack — O(V)
    cout << "Calculating slack times..." << endl;
    calculateSlackTimes();
 
    // Step 4: Critical path via backtracking — O(V) per path
    cout << "Finding critical path..." << endl;
    findCriticalPaths();
 
    calculateTotalDelay();
    calculateSlewTimes();
    calculateCapacitance();
    calculateFanoutCounts();
 
    cout << "Timing analysis completed!" << endl;
}
 

// FIX 1: Correct Kahn's algorithm for arrival times
// inDegree[node] = number of gate-input pairs that must be processed
//                  before this node can be processed

void TimingAnalyzer::calculateArrivalTimes() {
    unordered_map<string, int> inDegree;
 
    // Initialize all nodes to 0
    for (const auto& nodePair : circuit.getNodes()) {
        inDegree[nodePair.first] = 0;
    }
 
    // FIXED: output node needs ALL inputs of its driving gate processed first
    for (const auto& gate : circuit.getGates()) {
        inDegree[gate->getOutput()] += gate->getInputs().size();
    }
 
    // Primary inputs have in-degree 0 - seed the queue
    queue<string> processQueue;
    for (const auto& input : circuit.getPrimaryInputs()) {
        auto node = circuit.getNode(input);
        if (node) {
            node->setArrivalTimeRise(0.0);
            node->setArrivalTimeFall(0.0);
            arrivalTimes[input] = 0.0;
            processQueue.push(input);
        }
    }
 
    while (!processQueue.empty()) {
        string currentNode = processQueue.front();
        processQueue.pop();
 
        auto node = circuit.getNode(currentNode);
        if (!node) continue;
 
        for (const auto& gate : node->getFanouts()) {
            // Collect max arrival time across all gate inputs
            double maxInputArrival = 0.0;
            bool allInputsReady = true;
 
            for (const auto& input : gate->getInputs()) {
                auto it = arrivalTimes.find(input);
                if (it == arrivalTimes.end()) {
                    allInputsReady = false;
                    break;
                }
                maxInputArrival = max(maxInputArrival, it->second);
            }
 
            // Decrement counter; only process when all inputs are ready
            inDegree[gate->getOutput()]--;
            if (inDegree[gate->getOutput()] == 0) {
                double gateDelay = circuit.getGateDelay(gate->getType());
                gate->setDelay(gateDelay);
 
                vector<double> inputArrivals;
                for (const auto& input : gate->getInputs()) {
                    auto inputNode = circuit.getNode(input);
                    if (inputNode)
                        inputArrivals.push_back(inputNode->getMaxArrivalTime());
                }
 
                double outputArrival = gate->calculateDelay(inputArrivals);
                string outputName = gate->getOutput();
                auto outputNode = circuit.getNode(outputName);
                if (outputNode) {
                    outputNode->setArrivalTimeRise(outputArrival);
                    outputNode->setArrivalTimeFall(outputArrival);
                    arrivalTimes[outputName] = outputArrival;
                }
 
                processQueue.push(outputName);
            }
        }
    }
}
 

// FIX 2: Correct backward BFS for required times
// outDegree[node] = number of gates that use this node as input

void TimingAnalyzer::calculateRequiredTimes() {
    double clockPeriod = circuit.getClockPeriod();
    unordered_map<string, int> outDegree;
 
    // Initialize all nodes to 0
    for (const auto& nodePair : circuit.getNodes()) {
        outDegree[nodePair.first] = 0;
    }
 
    // FIXED: count how many gates use each node as an input
    for (const auto& gate : circuit.getGates()) {
        for (const auto& input : gate->getInputs()) {
            outDegree[input]++;
        }
    }
 
    // Primary outputs seed the backward queue
    queue<string> processQueue;
    for (const auto& output : circuit.getPrimaryOutputs()) {
        auto node = circuit.getNode(output);
        if (node) {
            node->setRequiredTimeRise(clockPeriod);
            node->setRequiredTimeFall(clockPeriod);
            requiredTimes[output] = clockPeriod;
            processQueue.push(output);
        }
    }
 
    while (!processQueue.empty()) {
        string currentNode = processQueue.front();
        processQueue.pop();
 
        auto node = circuit.getNode(currentNode);
        if (!node) continue;
 
        auto faninGate = node->getFanin();
        if (!faninGate) continue;
 
        double gateDelay = circuit.getGateDelay(faninGate->getType());
        double inputRequired = node->getMinRequiredTime() - gateDelay;
 
        for (const auto& input : faninGate->getInputs()) {
            auto inputNode = circuit.getNode(input);
            if (inputNode) {
                // Take the most conservative (minimum) required time
                double existing = inputNode->getMinRequiredTime();
                double updated  = min(existing, inputRequired);
                inputNode->setRequiredTimeRise(updated);
                inputNode->setRequiredTimeFall(updated);
                requiredTimes[input] = updated;
            }
 
            outDegree[input]--;
            if (outDegree[input] == 0) {
                processQueue.push(input);
            }
        }
    }
}
 

// FIX 3: O(V) critical path backtracking — no exponential enumeration
// Backtracks from the primary output with worst slack,
// choosing at each gate the input with the latest arrival time.

void TimingAnalyzer::findCriticalPaths() {
    criticalPaths.clear();
 
    // Find primary output with worst (most negative) slack
    string worstOutput;
    double worstSlackVal = numeric_limits<double>::max();
 
    for (const auto& output : circuit.getPrimaryOutputs()) {
        auto node = circuit.getNode(output);
        if (node && node->getWorstSlack() < worstSlackVal) {
            worstSlackVal = node->getWorstSlack();
            worstOutput   = output;
        }
    }
 
    if (worstOutput.empty()) return;
 
    // Backtrack from worst output to primary input
    TimingPath path;
    path.slack      = worstSlackVal;
    path.isCritical = (worstSlackVal <= 0.0);
 
    string current = worstOutput;
    while (true) {
        path.nodes.push_back(current);
        auto node = circuit.getNode(current);
        if (!node) break;
 
        auto faninGate = node->getFanin();
        if (!faninGate) break;  // reached primary input
 
        // Choose input with the latest arrival time
        string bestInput;
        double bestArrival = -1.0;
        for (const auto& input : faninGate->getInputs()) {
            auto it = arrivalTimes.find(input);
            if (it != arrivalTimes.end() && it->second > bestArrival) {
                bestArrival = it->second;
                bestInput   = input;
            }
        }
        if (bestInput.empty()) break;
        current = bestInput;
    }
 
    // Reverse so path runs input => output
    reverse(path.nodes.begin(), path.nodes.end());
    path.totalDelay = calculatePathDelay(path.nodes);
    criticalPaths.push_back(path);
}
 
void TimingAnalyzer::calculateSlackTimes() {
    for (const auto& nodePair : circuit.getNodes()) {
        const string& nodeName = nodePair.first;
        auto node = nodePair.second;
 
        double slack = node->getMinRequiredTime() - node->getMaxArrivalTime();
        node->setSlackRise(slack);
        node->setSlackFall(slack);
        slackTimes[nodeName] = slack;
    }
 
    updateWorstSlack();
}
 
double TimingAnalyzer::calculatePathDelay(const vector<string>& path) const {
    double total = 0.0;
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        auto node = circuit.getNode(path[i]);
        if (!node) continue;
        for (const auto& gate : node->getFanouts()) {
            if (gate->getOutput() == path[i + 1]) {
                total += circuit.getGateDelay(gate->getType());
                break;
            }
        }
    }
    return total;
}
 
void TimingAnalyzer::calculateTotalDelay() {
    totalDelay = 0.0;
    for (const auto& output : circuit.getPrimaryOutputs()) {
        auto it = arrivalTimes.find(output);
        if (it != arrivalTimes.end())
            totalDelay = max(totalDelay, it->second);
    }
}
 
void TimingAnalyzer::calculateSlewTimes() {
    for (const auto& nodePair : circuit.getNodes()) {
        auto node = nodePair.second;
        double t = node->getMaxArrivalTime();
        node->setSlewRise(t * 0.1);
        node->setSlewFall(t * 0.1);
    }
}
 
void TimingAnalyzer::calculateCapacitance() {
    for (const auto& nodePair : circuit.getNodes()) {
        auto node = nodePair.second;
        node->setCapacitance(1.0 + node->getFanouts().size() * 0.5);
    }
}
 
void TimingAnalyzer::calculateFanoutCounts() {
    for (const auto& nodePair : circuit.getNodes()) {
        auto node = nodePair.second;
        node->setFanoutCount(node->getFanouts().size());
    }
}
 
void TimingAnalyzer::updateWorstSlack() {
    worstSlack = numeric_limits<double>::max();
    for (const auto& p : slackTimes)
        worstSlack = min(worstSlack, p.second);
    if (worstSlack == numeric_limits<double>::max()) worstSlack = 0.0;
}
 
void TimingAnalyzer::resetAnalysis() {
    criticalPaths.clear();
    arrivalTimes.clear();
    requiredTimes.clear();
    slackTimes.clear();
    worstSlack  = 0.0;
    totalDelay  = 0.0;
    for (const auto& nodePair : circuit.getNodes())
        nodePair.second->resetTiming();
}
 
bool TimingAnalyzer::isTimingViolation() const {
    return worstSlack < 0.0;
}

void TimingAnalyzer::generateReport(const string& filename) {
    ofstream file(filename);
    if (!file.is_open()) {
        throw runtime_error("Cannot create report file: " + filename);
    }
    
    file << fixed << setprecision(3);
    file << "===========================================" << endl;
    file << "        STATIC TIMING ANALYSIS REPORT" << endl;
    file << "===========================================" << endl;
    file << endl;
    
    // Summary
    file << "SUMMARY:" << endl;
    file << "--------" << endl;
    file << "Clock Period: " << circuit.getClockPeriod() << " ns" << endl;
    file << "Total Delay: " << totalDelay << " ns" << endl;
    file << "Worst Slack: " << worstSlack << " ns" << endl;
    file << "Timing Violation: " << (isTimingViolation() ? "YES" : "NO") << endl;
    file << "Number of Paths: " << allPaths.size() << endl;
    file << "Critical Paths: " << criticalPaths.size() << endl;
    file << endl;
    
    // Node timing information
    file << "NODE TIMING INFORMATION:" << endl;
    file << "-----------------------" << endl;
    for (const auto& nodePair : circuit.getNodes()) {
        const string& name = nodePair.first;
        auto node = nodePair.second;
        
        file << "Node: " << name << endl;
        file << "  Arrival Time: " << node->getMaxArrivalTime() << " ns" << endl;
        file << "  Required Time: " << node->getMinRequiredTime() << " ns" << endl;
        file << "  Slack: " << node->getWorstSlack() << " ns" << endl;
        file << "  Slew: " << node->getSlewRise() << " ns" << endl;
        file << "  Capacitance: " << node->getCapacitance() << " fF" << endl;
        file << "  Fanout: " << node->getFanoutCount() << endl;
        file << endl;
    }
    
    // Critical paths
    if (!criticalPaths.empty()) {
        file << "CRITICAL PATHS:" << endl;
        file << "---------------" << endl;
        for (size_t i = 0; i < criticalPaths.size(); ++i) {
            const auto& path = criticalPaths[i];
            file << "Path " << (i + 1) << " (Slack: " << path.slack << " ns):" << endl;
            for (size_t j = 0; j < path.nodes.size(); ++j) {
                file << "  " << path.nodes[j];
                if (j < path.nodes.size() - 1) file << " -> ";
            }
            file << endl;
            file << "  Total Delay: " << path.totalDelay << " ns" << endl;
            file << endl;
        }
    }
    
    file.close();
}

void TimingAnalyzer::printSummary() {
    cout << fixed << setprecision(3);
    cout << "\n=== TIMING ANALYSIS SUMMARY ===" << endl;
    cout << "Clock Period: " << circuit.getClockPeriod() << " ns" << endl;
    cout << "Total Delay: " << totalDelay << " ns" << endl;
    cout << "Worst Slack: " << worstSlack << " ns" << endl;
    cout << "Timing Violation: " << (isTimingViolation() ? "YES" : "NO") << endl;
    cout << "Number of Paths: " << allPaths.size() << endl;
    cout << "Critical Paths: " << criticalPaths.size() << endl;
    
    if (!criticalPaths.empty()) {
        cout << "\nMost Critical Path:" << endl;
        printTimingPath(criticalPaths[0]);
    }
}

void TimingAnalyzer::printDetailedReport() {
    printSummary();
    
    cout << "\n=== DETAILED NODE TIMING ===" << endl;
    for (const auto& nodePair : circuit.getNodes()) {
        nodePair.second->printTiming();
        cout << endl;
    }
}

void TimingAnalyzer::printTimingPath(const TimingPath& path) const {
    cout << "Path (Slack: " << path.slack << " ns): ";
    
    // If the path is massive, just print the beginning and end
    if (path.nodes.size() > 10) {
        cout << path.nodes.front() << " -> ... [" 
             << path.nodes.size() - 2 << " internal gates] ... -> " 
             << path.nodes.back();
    } 
    // If it's a normal sized path, print the whole thing
    else {
        for (size_t i = 0; i < path.nodes.size(); ++i) {
            cout << path.nodes[i];
            if (i < path.nodes.size() - 1) cout << " -> ";
        }
    }
    cout << " (Delay: " << path.totalDelay << " ns)" << endl;
}

// ============================================================================
// MAIN FUNCTION
// ============================================================================
int main() {
    // Hardcoded file paths - we can change these to use different input files
    string circuitFile = "examples/massive_circuit.txt";
    string delayFile = "delays/gate_delays.txt";
    string outputFile = "reports/timing_report.txt";
   
    try {
        // Creating circuit and load configuration
        Circuit circuit;
        circuit.loadCircuit(circuitFile);
        circuit.loadDelays(delayFile);

        // Creating timing analyzer
        TimingAnalyzer analyzer(circuit);

        cout << "Performing Static Timing Analysis..." << endl;

        // --- START TIMER ---
        auto start = chrono::high_resolution_clock::now();

        // Performing timing analysis
        analyzer.analyze();

        // --- STOP TIMER ---
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);

        // Generating timing report
        analyzer.generateReport(outputFile);
        cout << "Timing analysis completed. Report saved to: " << outputFile << endl;

        // Printing summary to console
        analyzer.printSummary();

        // --- PRINTING METRICS ---
        cout << "\n====================================\n";
        cout << "Execution Time: " << duration.count() << " ms\n";
        cout << "====================================\n";

    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return 1;
    }

    return 0;
}
