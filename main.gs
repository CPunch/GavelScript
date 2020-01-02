function fact(i) {
    if (i == 1)
        return 1;
    return i*fact(i-1);
}

x = 5;
print("The factorial of ", x, " is ", fact(x));