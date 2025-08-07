module Main where

-- Factorial function (recursive, exercises I, K, A combinators)
fact :: Int -> Int
fact 0 = 1
fact n = n * fact (n - 1)

-- Sum function with pattern matching
sumList :: [Int] -> Int
sumList [] = 0
sumList (x:xs) = x + sumList xs

-- Map-based computation
compute :: Int -> Int
compute n = sumList (map fact [1..n])

main :: IO ()
main = do
  let result = compute 10
  print result
  print (fact 20)
  print (sumList [1..100])
