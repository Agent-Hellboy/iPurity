#ifndef AFC_CLIENT_POOL_H
#define AFC_CLIENT_POOL_H
#include <condition_variable>
#include <mutex>
#include <vector>

#include <libimobiledevice/afc.h>
#include <libimobiledevice/lockdown.h>



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
