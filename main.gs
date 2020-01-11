function fact(i) 
    if (i == 0) then
        return 1;
    end
    return i*fact(i-1);
end

tblTest = {0, 0, 0, 0, 0, "Hello!"};
x = 1;
while(x < 6) do
    tblTest[x-1] = fact(x);
    x=x+1;
end

x = 0;
while(x < 5) do
    print("The factorial of ", x+1, " is ", tblTest[x]);
    x=x+1;
end
print(tblTest[5]);