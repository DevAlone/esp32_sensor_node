#pragma once

void rootWriteWorker(void*);
void serverSenderWorker(void*);
void alternativeServerSenderWorker(void*);
void meshNodeReadWorker(void*);
void meshNodeWriteWorker(void*);

/**
 * @brief Timed printing system information
 */
void printSystemInfo(void*);
