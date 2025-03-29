int foo(int b)
{
    int i = 0;
    if (b == 1)
    {
        i = 1;
        i = foo(0);
    }
    else
    {
        i = 2;
    }
    return i;
}

int main()
{
    return foo(1);
}