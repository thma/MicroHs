module Main where

-- Simple test to trigger JIT compilation
-- This should execute I combinator many times

id :: a -> a
id x = x

main :: IO ()
main = do
  let result = foldr (+) 0 [1..10000]
  print result