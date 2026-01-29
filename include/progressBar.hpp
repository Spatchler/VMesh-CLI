#pragma once

#include <print>
#include <future>
#include <thread>
#include <chrono>
#include <cmath>

void printProgressBar(const std::string& pTitle, uint64_t* pCompletedCount, uint64_t pTotal, float pTotalInv, uint8_t pPrimaryEscapeColour = 31, uint8_t pSecondaryEscapeColour = 30, uint pWidth = 20);

void printProgressBarUntilDone(std::mutex* pSTDOUTMutex, const std::string& pTitle, uint64_t* pCompletedCount, uint64_t pTotal, uint8_t pPrimaryEscapeColour = 31, uint8_t pSecondaryEscapeColour = 30, uint pWidth = 20);

std::future<void> startProgressBar(std::mutex* pSTDOUTMutex, const std::string& pTitle, uint64_t* pCompletedCount, uint64_t pTotal, uint8_t pPrimaryEscapeColour = 31, uint8_t pSecondaryEscapeColour = 30, uint pWidth = 20);

