#include <iostream>
#include <string>

int main()
{
  std::string name;
  std::cout << "What is the pin name? ";
  getline (std::cin, name);
  std::cout << "Hello, pin : " << name << "!\n";
}
