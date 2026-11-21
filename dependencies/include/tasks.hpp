#pragma once

#include "rang.hpp"

#include <print>
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <thread>
#include <mutex>
#include <string>

using namespace rang;

namespace Tasks {
  class Timer {
  public:
    Timer() {
      start();
    }
  
    std::chrono::duration<double> getTime() {
      return std::chrono::system_clock::now() - mStartTime;
    }

    void start() {
      mStartTime = std::chrono::system_clock::now();
    }
  protected:
    std::chrono::time_point<std::chrono::system_clock> mStartTime;
  };

  class ScopedTimer: public Timer {
  public:
    ScopedTimer(const std::string& pName) {
      mName = pName;
      start();
      std::println("'{0}' timer started", mName);
    }
  
    ~ScopedTimer() {
      std::println("'{0}' timer stopped: {1}", mName, getTime());
    }
  protected:
    std::string mName;
  };

  std::string getStyle(style pStyle) { return std::format("\033[{}m", static_cast<int>(pStyle)); }
  std::string getStyle(fg pStyle)    { return std::format("\033[{}m", static_cast<int>(pStyle)); }
  std::string getStyle(bg pStyle)    { return std::format("\033[{}m", static_cast<int>(pStyle)); }
  std::string getStyle(fgB pStyle)   { return std::format("\033[{}m", static_cast<int>(pStyle)); }
  std::string getStyle(bgB pStyle)   { return std::format("\033[{}m", static_cast<int>(pStyle)); }

  class Task {
  public:
    Task(const std::string &pName)
    :mName(pName) {
    }

    void lockSubTasks() {
      mSubTasksMutex.lock();
      for (auto& i: mSubTasks)
        i->lockSubTasks();
    }

    void unlockSubTasks() {
      mSubTasksMutex.unlock();
      for (auto& i: mSubTasks)
        i->unlockSubTasks();
    }

    void addSubTask(std::shared_ptr<Task> pTask) {
      std::lock_guard<std::mutex> lock(mSubTasksMutex);
      mSubTasks.emplace_back(pTask);
    }

    uint getNumLines() {
      uint numLines = 1;
      for (auto& s: mSubTasks)
        numLines += s->getNumLines();
      return numLines;
    }

    void begin() {
      mIsStarted = true;
      mTimer.start();
    }

    void tick(std::vector<bool> pIsLast) {
      if (mIsComplete) mText = mCompletedText;
      else if (!mIsStarted) mText = mName;
      else tickImpl();
      mAllText = mText;
      if (mSubTasks.size() == 0) return;
      for (int i = 0; i < mSubTasks.size(); ++i) {
        pIsLast.push_back(i == mSubTasks.size()-1);
        mSubTasks[i]->tick(pIsLast);
        pIsLast.pop_back();
        // mAllText += "\n";
        // mAllText += pIsLast[0] ? "     " : " │   ";
        // for (uint j = 1; j < pIsLast.size(); ++j) mAllText += pIsLast[j] ? "    " : "│   ";
        // mAllText +=
        //   i == mSubTasks.size()-1
        //   ? std::format("{}└{} {}", getStyle(fgB::gray), getStyle(fg::reset), mSubTasks[i]->getText())
        //   : std::format("{}├{} {}", getStyle(fgB::gray), getStyle(fg::reset), mSubTasks[i]->getText());
        mAllText += "\n";
        mAllText += std::format("{}", mSubTasks[i]->getText());
      }
    }

    std::chrono::duration<double> getTime() {
      if (!mIsStarted) return std::chrono::seconds(0);
      if (mIsComplete) return mCompletedTime;
      return mTimer.getTime();
    }

    std::string getText() {
      return mAllText;
    }

    constexpr bool isComplete() { return mIsComplete; }

    void setName(const std::string& pName) { mName = pName; }

    constexpr std::string getName() { return mName; }

    void complete(bool pSuccess) {
      mIsComplete = true;
      mCompletedTime = mTimer.getTime();
      mCompletedText =
        pSuccess
        ? std::format("{}✔{} {}", getStyle(fg::green), getStyle(fg::reset), getCompletedText(pSuccess))
        : std::format("{}✖{} {}", getStyle(fg::red), getStyle(fg::reset), getCompletedText(pSuccess));
    }
  protected:
    virtual std::string getCompletedText(bool pSuccess) {
      std::string text = mName;
      std::string time = std::format("{}", getTime());
      for (uint i = text.size(); i < (100 - time.size()); ++i)
        text += " ";
      text += time;
      return text;
    }

    virtual void tickImpl() {
      mText = std::format("{}...", mName);
    }
    std::string mName;
    std::string mText;
    std::vector<std::shared_ptr<Task>> mSubTasks;
  private:
    std::mutex mSubTasksMutex;
    std::string mCompletedText;
    std::chrono::duration<double> mCompletedTime;
    std::atomic<bool> mIsComplete {false};
    std::atomic<bool> mIsStarted {false};
    std::string mAllText;
    Timer mTimer;
  };

  class SpinnerTask: public Task {
  public:
    SpinnerTask(const std::string& pName)
    :Task(pName) {
    }

    void setSpinnerStates(const std::vector<std::string>& pSpinnerStates) {
      mSpinnerStates = pSpinnerStates;
    }
  protected:
    void tickImpl() override {
      mText = std::format("{}{}{} {}...", getStyle(fg::magenta), mSpinnerStates[++mSpinnerStateIndex % mSpinnerStates.size()], getStyle(fg::reset), mName);
    }

    uint8_t mSpinnerStateIndex {0};
    std::vector<std::string> mSpinnerStates {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
  };

  class ProgressBarTask: public SpinnerTask {
  public:
    ProgressBarTask(const std::string& pName, std::atomic<uint64_t>* pCounter, uint64_t pTotal, uint pBarWidth)
    :SpinnerTask(pName),
    mCounter(pCounter),
    mTotal(pTotal),
    mTotalInv(pTotal == 0 ? 0 : 1./pTotal),
    mBarWidth(pBarWidth)
    {}

    void setCounter(std::atomic<uint64_t>* pCounter) {
      mCounter = pCounter;
    }

    void setTotal(uint64_t pTotal) {
      mTotal = pTotal;
      mTotalInv = pTotal == 0 ? 0 : 1./pTotal;
    }
  protected:
    std::string getCompletedText(bool pSuccess) override {
      // std::string text = std::format("{} 100% {} {} / {}", mName, getBar(mBarWidth, mBarWidth), mTotal, mTotal);
      std::string text = std::format("{} 100% {} / {}", mName, mTotal, mTotal);
      std::string time = std::format("{}", getTime());
      for (uint i = text.size(); i < (100 - time.size()); ++i)
        text += " ";
      text += time;
      return text;
    }

    void tickImpl() override {
      double percent {mCounter == nullptr ? 0 : mCounter->load() * mTotalInv};
      percent = percent * percent * percent * percent * percent * percent * percent;
      int ipercent {static_cast<int>(percent*100)};
      double barPercent = percent * mBarWidth;
      // double barPercent {mCounter == nullptr ? 0 : mCounter->load() * mBarMultiplier};
      int iBarPercent {static_cast<int>(barPercent)};
      mText = std::format("{}{}{} {} {}% {} {} / {}", getStyle(fg::magenta), mSpinnerStates[++mSpinnerStateIndex % mSpinnerStates.size()], getStyle(fg::reset), mName, ipercent, getBar(iBarPercent, barPercent), mCounter->load(), mTotal);
    }

    std::string getBar(int pIPercent, double pDPercent) {
      double diff = pDPercent - static_cast<double>(pIPercent);
      // std::array<char, 4> partial {'░','▒','▓','█'};
      std::array<std::string, 4> partial {"░","▒","▓","█"};
      // std::array<std::string, 7> partial {"⣀","⣄","⣤","⣦","⣶","⣷","⣿"};
      // std::string partial {"░▒▓█"};
      std::string bar {""};
      bar += getStyle(fg::magenta);
      for (uint i = 0; i <= pIPercent; ++i)
        bar += partial.back();
        // bar += "━";
        // bar += "█";
        // bar += "￭";
      if (pIPercent != mBarWidth) bar += partial.at(diff * partial.size());
      bar += getStyle(fg::reset);
      for (uint i = pIPercent; i < mBarWidth - 1; ++i)
        bar += partial.at(0);
        // bar += "━";
        // bar += "░";
        // bar += "・";
      bar += getStyle(fg::reset);
      return bar;
    }

    std::atomic<uint64_t>* mCounter;
    uint64_t mTotal;
    double mTotalInv;
    uint mBarWidth;
  };

  class TaskList {
  public:
    TaskList() = default;
    TaskList(const std::vector<std::shared_ptr<Task>>& pTasks)
    :mTasks(pTasks) {
    }

    void addTask(std::shared_ptr<Task> pTask) {
      std::lock_guard<std::mutex> lock(mTasksMutex);
      mTasks.emplace_back(pTask);
    }

    void setUpdateFrequency_ms(uint pF) {
      mUpdateFrequency_ms = pF;
    }

    void begin() {
      auto job = [this]() {
        for (; !this->mIsComplete.load();) {
          std::this_thread::sleep_for(std::chrono::milliseconds(this->mUpdateFrequency_ms.load()));
          this->tickPrint();
        }
      };
      mLogThread = new std::thread(job);
    }

    void tickPrint() {
      std::lock_guard<std::mutex> lock1(mTasksMutex);
      if (mTasks.size() == 0) return;
      moveCursorUp(mPreviousNumLines);
      mPreviousNumLines = 1;
      for (uint i = 0; i < mTasks.size(); ++i) {
        auto& j {mTasks[i]};
        j->lockSubTasks();
        j->tick({i == mTasks.size()-1});
        std::lock_guard<std::mutex> lock2(mLogMutex);
        // if (i == mTasks.size() - 1) std::println(" └ {}", j->getText());
        // else std::println(" ├ {}", j->getText());
        std::println("{}", j->getText());
        mPreviousNumLines += j->getNumLines();
        j->unlockSubTasks();
      }
    }

    void lockLogMutex() {
      mLogMutex.lock();
    }

    void unlockLogMutex() {
      mLogMutex.unlock();
    }

    void markComplete() {
      mIsComplete.store(true);
    }

    // bool isComplete() {
    //   if (mLogThread == nullptr) return true;
    //   return mLogThread->joinable();
    // }

    void wait() {
      mLogThread->join();
      delete mLogThread;
      mLogThread = nullptr;
    }

    void stop() {
      markComplete();
      wait();
    }
  private:
    static inline void moveCursorUp(uint lines) { std::println("\033[{}A", lines); }

    std::vector<std::shared_ptr<Task>> mTasks;
    std::mutex mTasksMutex;
    uint mPreviousNumLines {0};
    std::thread* mLogThread {nullptr};
    std::atomic<bool> mIsComplete {false};
    std::atomic<uint> mUpdateFrequency_ms {60};
    bool mFirstPrint {true};
    std::mutex mLogMutex;
  };
}
