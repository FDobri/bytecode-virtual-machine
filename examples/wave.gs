# Score a six-enemy wave, with escalating values for the last three enemies.
fn main() {
    let index = 0;
    let score = 0;
    while index < 6 {
        score = score + points(index);
        index = index + 1;
    }
    return score;
}

fn points(index) {
    if index < 3 {
        return 10;
    } else {
        return 20 + (index - 3) * 5 + 35 / 3;
    }
}
