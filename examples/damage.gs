fn strike(health, armor, damage) {
    let impact = damage - armor;
    if impact < 1 { impact = 1; }
    if health < impact { return 0; }
    return health - impact;
}

fn main() {
    return strike(strike(50, 7, 24), 7, 21);
}
