#include "progressBar.hpp"

void printProgressBar(const std::string& pTitle, uint64_t* pCompletedCount, uint64_t pTotal, float pTotalInv, std::mutex* pSTDOUTMutex, uint8_t pPrimaryEscapeColour, uint8_t pSecondaryEscapeColour, uint pWidth) {
  std::lock_guard<std::mutex> lock(*pSTDOUTMutex);

  float progress = *pCompletedCount * pTotalInv;
  // std::print("\x1b[1F\x1b[2K{}: ", pTitle);
  std::print("{}: ", pTitle);
  // std::print("\x1b 8\x1b 7{} ", pTitle);
  float threshold = std::ceil(progress * pWidth);

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
  else std::print("\x1b[49;{}m", pPrimaryEscapeColour);

  // std::println("\x1b[39m {}%, [ {} / {} ]", std::ceil(progress*100), *pCompletedCount, pTotal); // Reset
  std::print("\x1b[39m {}%, [ {} / {} ]\r", std::ceil(progress*100), *pCompletedCount, pTotal); // Reset

  std::cout.flush();
}

static bool checkIfDoneForTime(uint64_t* pCompletedCount, uint64_t pTotal, std::chrono::duration<double, std::milli> pDuration) {
  std::chrono::time_point<std::chrono::system_clock> startTime = std::chrono::system_clock::now();
  while (std::chrono::duration<double, std::milli>(std::chrono::system_clock::now() - startTime).count() < pDuration.count())
    if (*pCompletedCount == pTotal) return true;
  return false;
}

void printProgressBarUntilDone(std::mutex* pSTDOUTMutex, const std::string& pTitle, uint64_t* pCompletedCount, uint64_t pTotal, uint8_t pPrimaryEscapeColour, uint8_t pSecondaryEscapeColour, uint pWidth) {
  float totalInv = 1.f/pTotal;
  printProgressBar(pTitle, pCompletedCount, pTotal, totalInv, pSTDOUTMutex, pPrimaryEscapeColour, pSecondaryEscapeColour, pWidth);
  while (!checkIfDoneForTime(pCompletedCount, pTotal, std::chrono::milliseconds(1000)))
    printProgressBar(pTitle, pCompletedCount, pTotal, totalInv, pSTDOUTMutex, pPrimaryEscapeColour, pSecondaryEscapeColour, pWidth);
  printProgressBar(pTitle, pCompletedCount, pTotal, totalInv, pSTDOUTMutex, pPrimaryEscapeColour, pSecondaryEscapeColour, pWidth);
  {
    std::lock_guard<std::mutex> lock(*pSTDOUTMutex);
    // std::println("Finished {}", pTitle);
    std::println("");
  }
}

std::future<void> startProgressBar(std::mutex* pSTDOUTMutex, const std::string& pTitle, uint64_t* pCompletedCount, uint64_t pTotal, uint8_t pPrimaryEscapeColour, uint8_t pSecondaryEscapeColour, uint pWidth) {
  return std::async(std::launch::async, printProgressBarUntilDone, pSTDOUTMutex, pTitle, pCompletedCount, pTotal, pPrimaryEscapeColour, pSecondaryEscapeColour, pWidth);
}

