import std;

int main()
{
    std::cout << "hello world" << std::endl;

    std::vector<std::string> vec;
    vec.emplace_back("hello world");
    std::cout << vec.size() << std::endl;

    std::unordered_map<int, int> mp;
    mp.emplace(1, 2);
    std::cout << mp.size() << std::endl;

    auto it =
        std::ranges::find_if(vec, [](auto &&p) { return p == "hello world"; });
    if (it != vec.end())
    {
        std::cout << "found" << std::endl;
    }

    std::unordered_set<int> s;
    s.insert(100);
    std::cout << s.size() << std::endl;

    std::shared_ptr<std::string> ptr_1;
    std::unique_ptr<std::string> ptr_2;

    return 0;
}
