function fact(i) 
    a = i;
    while i-- > 0 do
        a = a * i;
    end
    return a;
end

// this looks so much like lua now oh god

z = 1001;

while z-- > 0 do
    x = 0;
    while x++ <= 100 do
        print("The factorial of ", x, " is ", fact(x));
    end
end
