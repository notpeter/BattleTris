#ifndef BT_TEST_DROP_H
#define BT_TEST_DROP_H
#include "Game.H"
#include "Match.H"

// Placement fixtures wait for the animated drop and landing window.
inline void finishDrop(BrowserGame &game) {
  const unsigned generation = game.generation;
  game.input(4);
  for (int i = 0; i < 1000 && !game.paused && !game.over && game.generation == generation; ++i)
    game.tick(10, false);
}
inline void finishDrop(BrowserMatch &match) {
  const unsigned generation = match.player.generation;
  match.input(4);
  for (int i = 0; i < 1000 && match.status() == 0 && match.player.generation == generation; ++i)
    match.tick(10);
}
inline bool finishDrop(BrowserMatch &match, int side) {
  const unsigned generation = (side ? match.opponent : match.player).generation;
  const bool accepted = match.sideInput(side, 4);
  for (int i = 0; accepted && i < 1000 && match.status() == 0 &&
       (side ? match.opponent : match.player).generation == generation; ++i)
    match.tick(10);
  return accepted;
}
#endif
