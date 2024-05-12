from copy import deepcopy
import math

board_size = 4

default_traversal = list(range(board_size))
reversed_traversal = list(reversed(default_traversal))

traversals = [
  [
    default_traversal,
    default_traversal,
  ],
  [
    reversed_traversal,
    default_traversal,
  ],
  [
    default_traversal,
    reversed_traversal,
  ],
  [
    default_traversal,
    default_traversal,
  ],
]

vectors = [
  [0,  -1],
  [1, 0],
  [0, 1],
  [-1, 0],
]

def try_move(board, direction):
  board_copy = deepcopy(board)

  traversal = traversals[direction]
  vector = vectors[direction]

  merged_from = [[None] * board_size] * board_size

  for x in traversal[0]:
    for y in traversal[1]:
      cell = [x, y]
      tile = board_copy[x][y]

      if tile:
        while True:
          prev = cell.copy()
          cell[0] += vector[0]
          cell[1] += vector[1]
          if not (cell[0] >= 0 and cell[0] < board_size and cell[1] >= 0 and cell[1] < board_size and not board_copy[cell[0]][cell[1]]):
            break


        if cell[0] >= 0 and cell[0] < board_size and cell[1] >= 0 and cell[1] < board_size and board_copy[cell[0]][cell[1]] == tile and not merged_from[cell[0]][cell[1]]:
          merged_from[cell[0]][cell[1]] = True

          board_copy[cell[0]][cell[1]] = 2 * tile
          board_copy[x][y] = 0
        else:
          board_copy[x][y] = 0
          board_copy[prev[0]][prev[1]] = tile
  return board_copy


f = open("lut.txt", "w")
s = ""

def log2(x):
  return int(math.log2(x)) if x != 0 else 0

for i in range(0, 65536):
  x1 = (i & 0xF000)>>12;
  x2 = (i & 0x0F00)>>8;
  x3 = (i & 0x00F0)>>4;
  x4 = i & 0x000F;
  row = [2**x1, 2**x2, 2**x3, 2**x4];
  row = [r if r!=1 else 0 for r in row]
  board = [row] + [[0]*4]*3
  right = [log2(x) for x in try_move(board, 2)[0]]
  left = [log2(x) for x in try_move(board, 0)[0]]

  res1 = (right[0]<<12) + (right[1]<<8)+(right[2]<<4)+right[3];
  res2 = (left[0]<<12) + (left[1]<<8 )+ (left[2]<<4) + left[3];
  res3 = 0
  i = 0
  for x in row:
    if x == 0:
      res3 += 2**i
    i += 1
  line = str(res1) + " " + str(res2) + " " + str(res3) + "\n"

  s+=line

f.write(s)
f.close()
