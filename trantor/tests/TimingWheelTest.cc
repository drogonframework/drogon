#include <trantor/utils/TimingWheel.h>
#include <trantor/utils/Logger.h>
#include <memory>
class MyClass
{
  public:
    MyClass();
    MyClass(MyClass &&) = default;
    MyClass(const MyClass &) = default;
    MyClass &operator=(MyClass &&) = default;
    MyClass &operator=(const MyClass &) = default;
    ~MyClass();

  private:
};

MyClass::MyClass()
{
}

MyClass::~MyClass()
{
    LOG_DEBUG << "MyClass destructed!";
}

int main()
{
    LOG_DEBUG << "start";
    trantor::EventLoop loop;
    std::weak_ptr<MyClass> weakEntry;
    trantor::TimingWheel wheel(&loop, 75, 0.1, 100);
    {
        auto entry = std::shared_ptr<MyClass>(new MyClass);

        wheel.insertEntry(75, entry);
        weakEntry = entry;
    }
    // loop.runAfter(5.0, [&]() {
    //     wheel.insertEntry(10, weakEntry.lock());
    // });
    // loop.runAfter(0.5,[&](){
    //     wheel.insertEntry(28,weakEntry.lock());
    // });
    // loop.runEvery(1.0,[](){
    //     LOG_DEBUG<<"tick";
    // });
    loop.loop();
}
