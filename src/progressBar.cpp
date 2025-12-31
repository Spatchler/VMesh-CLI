#include "progressBar.hpp"

void printProgressBar(float pProgress, const std::string& pTitle, uint8_t pPrimaryEscapeColour, uint8_t pSecondaryEscapeColour, uint pWidth) {
  std::print("\x1b[1F\x1b[2K{} ", pTitle);
  // std::print("\x1b 8\x1b 7{} ", pTitle);
  float threshold = std::ceil(pProgress * pWidth);

  // std::print("\x1b[u\x1b[s{} ", pTitle);
  // std::print("\r{} ", pTitle);
  std::print("\x1b[{}m\x1b[{}m", pPrimaryEscapeColour, pPrimaryEscapeColour + 10);

  for (uint i = 0; i < threshold; ++i)
    std::print(" ");

  if (threshold <= pWidth - 1) {
    std::print("\x1b[{}m\x1b[{}m", pSecondaryEscapeColour + 10, pSecondaryEscapeColour);

    for (uint i = 0; i < pWidth - threshold - 1; ++i)
      std::print(" ");

    std::print("\x1b[49m");
  }
  else
    std::print("\x1b[49;{}m", pPrimaryEscapeColour);

  std::println("\x1b[39m {}%", std::floor(pProgress*100)); // Reset
}

static bool checkIfDoneForTime(const std::function<float()>& pGetProgressFunc, std::chrono::duration<double, std::milli> pDuration) {
  std::chrono::time_point<std::chrono::system_clock> startTime = std::chrono::system_clock::now();
  while (std::chrono::duration<double, std::milli>(std::chrono::system_clock::now() - startTime).count() < pDuration.count()) {
    if (std::floor(pGetProgressFunc()*100) >= 100)
      return true;
  }
  return false;
}

void printProgressBarUntilDone(std::mutex* pSTDOUTMutex, const std::function<float()>& pGetProgressFunc, const std::string& pTitle, uint8_t pPrimaryEscapeColour, uint8_t pSecondaryEscapeColour, uint pWidth) {
  {
    std::lock_guard<std::mutex> lock(*pSTDOUTMutex);
    std::println("\x1b[s");
  }
  for (;;) {
    {
      std::lock_guard<std::mutex> lock(*pSTDOUTMutex);
      printProgressBar(pGetProgressFunc(), pTitle, pPrimaryEscapeColour, pSecondaryEscapeColour, pWidth);
    }
    if (checkIfDoneForTime(pGetProgressFunc, std::chrono::milliseconds(50))) {
      std::lock_guard<std::mutex> lock(*pSTDOUTMutex);
      printProgressBar(pGetProgressFunc(), pTitle, pPrimaryEscapeColour, pSecondaryEscapeColour, pWidth);
      break;
    }
  }
  std::println("Finished {}", pTitle);
}

std::future<void> startProgressBar(std::mutex* pSTDOUTMutex, const std::function<float()>& pGetProgressFunc, const std::string& pTitle, uint8_t pPrimaryEscapeColour, uint8_t pSecondaryEscapeColour, uint pWidth) {
  return std::async(std::launch::async, printProgressBarUntilDone, pSTDOUTMutex, pGetProgressFunc, pTitle, pPrimaryEscapeColour, pSecondaryEscapeColour, pWidth);
}

