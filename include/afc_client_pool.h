#ifndef AFC_CLIENT_POOL_H
#define AFC_CLIENT_POOL_H
#include <libimobiledevice/afc.h>
#include <libimobiledevice/lockdown.h>

#include <iostream>
#include <condition_variable>
#include <mutex>
#include <vector>

class AfcClientPool {
   public:
    AfcClientPool(idevice_t device, int poolSize);
    ~AfcClientPool();
    afc_client_t acquire();
    void release(afc_client_t client);

   private:
    idevice_t device_;
    std::vector<afc_client_t> pool_;
    std::mutex mtx_;
    std::condition_variable cv_;
};

#endif  // AFC_CLIENT_POOL_H
