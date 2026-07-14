clang++ -std=c++20 evagen.cpp -o evagen.exe `
    -I"C:\Program Files\LLVM\include" `
    -L"C:\Program Files\LLVM\lib" -llibclang `
	-D_CRT_SECURE_NO_WARNINGS