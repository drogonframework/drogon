#include <drogon/utils/monitoring/Histogram.h>
using namespace drogon;
using namespace drogon::monitoring;

void Histogram::observe(double value)
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (maxAge_ > std::chrono::seconds(0) &&
        timerId_ == trantor::InvalidTimerId)
    {
        std::weak_ptr<Histogram> weakPtr =
            std::dynamic_pointer_cast<Histogram>(shared_from_this());
        timerId_ = loopPtr_->runEvery(maxAge_ / timeBucketCount_, [weakPtr]() {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
                return;
            thisPtr->rotateTimeBuckets();
        });
    }
    auto &currentBucket = timeBuckets_.back();
    currentBucket.sum += value;
    currentBucket.count += 1;
    for (size_t i = 0; i < bucketBoundaries_.size(); i++)
    {
        if (value <= bucketBoundaries_[i])
        {
            currentBucket.buckets[i] += 1;
            break;
        }
    }
    if (value > bucketBoundaries_.back())
    {
        currentBucket.buckets.back() += 1;
    }
}

std::vector<Sample> Histogram::collect() const
{
    std::vector<Sample> samples;
    std::lock_guard<std::mutex> guard(mutex_);
    size_t count{0};
    for (size_t i = 0; i < bucketBoundaries_.size(); i++)
    {
        Sample sample;
        for (auto &bucket : timeBuckets_)
        {
            count += bucket.buckets[i];
        }
        sample.name = name_ + "_bucket";
        sample.exLabels.emplace_back("le",
                                     std::to_string(bucketBoundaries_[i]));
        sample.value = count;
        samples.emplace_back(std::move(sample));
    }
    Sample sample;
    for (auto &bucket : timeBuckets_)
    {
        count += bucket.buckets.back();
    }
    sample.name = name_ + "_bucket";
    sample.exLabels.emplace_back("le", "+Inf");
    sample.value = count;
    samples.emplace_back(std::move(sample));
    double sum{0};
    uint64_t totalCount{0};
    for (auto &bucket : timeBuckets_)
    {
        sum += bucket.sum;
        totalCount += bucket.count;
    }
    Sample sumSample;
    sumSample.name = name_ + "_sum";
    sumSample.value = sum;
    samples.emplace_back(std::move(sumSample));
    Sample countSample;
    countSample.name = name_ + "_count";
    countSample.value = totalCount;
    samples.emplace_back(std::move(countSample));
    return samples;
}
