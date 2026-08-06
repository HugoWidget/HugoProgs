#include <iostream>

int main()
{
    std::cout << "See https://github.com/HugoWidget/HugoFrzDrvHook\n"
	<< "Press 'g' to open the GitHub page in your default browser." << std::endl;
	char ch = std::cin.get();
	if (ch == 'g' || ch == 'G') {
		system("start https://github.com/HugoWidget/HugoFrzDrvHook");
	}
}