#include "afc_client_pool.h"


AfcClientPool::AfcClientPool(idevice_t device, int poolSize) : device_(device) {
    for (int i = 0; i < poolSize; i++) {
        afc_client_t client = nullptr;
        if (afc_client_start_service(device_, &client, "afc_scanner") ==
            AFC_E_SUCCESS) {
            pool_.push_back(client);
        } else {
            std::cerr << "Failed to create AFC client " << i << std::endl;
        }
    }
}

AfcClientPool::~AfcClientPool() {
    for (auto client : pool_) {
        afc_client_free(client);
    }
}

afc_client_t AfcClientPool::acquire() {
    std::unique_lock<std::mutex> lock(mtx_);
    // Add a timeout to prevent indefinite waiting
    if (!cv_.wait_for(lock, std::chrono::seconds(30), [this]() { return !pool_.empty(); })) {
        throw std::runtime_error("Timeout waiting for available AFC client");
    }
    afc_client_t client = pool_.back();
    pool_.pop_back();
    return client;
}

void AfcClientPool::release(afc_client_t client) {
    {
        std::lock_guard<std::mutex> lock(mtx_);
        pool_.push_back(client);
    }
    cv_.notify_one();
}
