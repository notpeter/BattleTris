#include "Recon.H"
#include "BTBoardManager.H"
#include <algorithm>
#include <limits>

void BrowserRecon::clearReport() {
  cells_.fill(0);
  funds_ = 0;
  known_ = false;
}

void BrowserRecon::deactivate() {
  token_ = -1;
  remaining_ = 0;
  clearReport();
}

void BrowserRecon::reset(unsigned seed) {
  random_.seed(seed);
  deactivate();
}

void BrowserRecon::activate(int token, int duration, bool preserveReport) {
  if ((token != BT_AMES && token != BT_ACE && token != BT_CONDOR) || duration <= 0) return;
  token_ = token;
  remaining_ += std::min(duration, 1000000000 - remaining_);
  // Even an upgrade has no fresh knowledge until the next settled-board report.
  if (!preserveReport) clearReport();
}

void BrowserRecon::clearedLines(int count) {
  if (count <= 0 || !remaining_) return;
  remaining_ = count >= remaining_ ? 0 : remaining_ - count;
  if (!remaining_) {
    token_ = -1;
    clearReport();
  }
}

std::uint64_t BrowserRecon::uniform(std::uint64_t bound) {
  // Two 31-bit draws cover abs(INT_MIN)+1 possible amplitudes safely. Rejection
  // avoids modulo bias for inclusive noise bounds that aren't powers of two.
  const std::uint64_t range = std::uint64_t(1) << 62;
  const std::uint64_t limit = range - range % bound;
  std::uint64_t draw;
  do {
    draw = (std::uint64_t(random_.integer()) << 31) | std::uint64_t(random_.integer());
  } while (draw >= limit);
  return draw % bound;
}

void BrowserRecon::report(BTBoardManager &board, int funds, int cleared) {
  if (!remaining_) return;
  cells_.fill(0);
  const double reveal = token_ == BT_AMES ? 0.5 : token_ == BT_ACE ? 0.85 : 1.0;
  for (int y = 0; y < BT_BOARD_HGT; ++y)
    for (int x = 0; x < BT_BOARD_WTH; ++x) {
      const int id = board.cell(x, y);
      // Twilight (-1) and Bug Report (0) remain hidden even from Condor.
      if (id > 0 && (reveal == 1.0 || random_.unit() < reveal))
        cells_[y * BT_BOARD_WTH + x] = id;
    }
  std::int64_t estimate = funds;
  if (token_ == BT_AMES || (token_ == BT_ACE && cleared == 4)) {
    const std::int64_t magnitude = estimate < 0 ? -estimate : estimate;
    const std::uint64_t maximum = token_ == BT_AMES ? std::uint64_t(magnitude) : 99;
    const std::int64_t noise = static_cast<std::int64_t>(uniform(maximum + 1));
    estimate += (random_.integer() & 1) ? noise : -noise;
  }
  estimate = std::max<std::int64_t>(std::numeric_limits<int>::min(),
    std::min<std::int64_t>(std::numeric_limits<int>::max(), estimate));
  funds_ = static_cast<int>(estimate);
  known_ = true;
}

int BrowserRecon::cell(int x, int y) const {
  if (x < 0 || y < 0 || x >= BT_BOARD_WTH || y >= BT_BOARD_HGT) return 0;
  return cells_[y * BT_BOARD_WTH + x];
}
