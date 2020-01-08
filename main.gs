function fact(i) 
    if (i == 0) then
        return 1;
    end
    return i*fact(i-1);
end

x = 5;
print("The factorial of ", x, " is ", fact(x));