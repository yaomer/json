使用时只需要简单包含`json.h`即可。

然后需要使用`c++17`来编译。

```
$ clang++ -std=c++17 json-check.cc
```

[json-checker](https://www.json.org/JSON_checker/)用于测试parse是否能正常工作。

---
假如我们有以下`json`文件，就叫做`m.json`吧。
```
{
    "name": "Bob",
    "age": 32,
    "langs": [
        "java",
        "go",
        "c"
    ],
    "scores": {
        "math": "A",
        "physics": "A",
        "computer": "B"
    }
}
```
parse
---
```cpp
#include "json.h"

#include <iostream>

using namespace std;

int main()
{
    json::value v;
    v.parsefile("m.json");
    cout << v.at("name").as_string().c_str() << "\n";
    cout << v.at("age").as_number() << "\n";
    cout << v.at("langs").at(0).as_string().c_str() << "\n";
    cout << v.at("langs").at(1).as_string().c_str() << "\n";
    cout << v.at("langs").at(2).as_string().c_str() << "\n";
    cout << v.at("scores").at("math").as_string().c_str() << "\n";
    cout << v.at("scores").at("physics").as_string().c_str() << "\n";
    cout << v.at("scores").at("computer").as_string().c_str() << "\n";
}
```
使用`[]`和`at`没有什么不同，只不过我觉得at从视觉上更一致些。

你也可以像使用`stl`那样遍历`数组`和`对象`。
```cpp
int main()
{
    json::value v;
    v.parsefile("m.json");
    for (auto& lang : v.at("langs").as_array()) {
        cout << lang->as_string().c_str() << "\n";
    }
    for (auto& [name, score] : v.at("scores").as_object()) {
        cout << name.c_str() << ", " << score->as_string().c_str() << "\n";
    }
}
```
dump
---
```cpp
int main()
{
    json::value v;
    v.at("name") = "Bob";
    v.at("age") = 32;
    v.at("langs") = { "java", "go", "c" };
    v.at("scores").at("math") = "A";
    v.at("scores").at("physics") = "A";
    v.at("scores").at("computer") = "B";
    string s;
    v.dump(s, 2); // 可视化输出，以2空格缩进。
    cout << s.c_str() << "\n";
}
```
对于`数组`，你也可以使用`append`，但不能像`对象`那样`at(i) = val`。
```cpp
int main()
{
    json::value v;
    v.append("A").append("B").append("C"); // true
    v.at(0) = "A"; // false
}
```
