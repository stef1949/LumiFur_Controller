import pytest

# Mirror the C++ constants
MIC_THRESHOLD = 450000
RANGE = 10000

def map_acc_to_brightness(acc: int) -> int:
    """
    Map raw accumulated mic value to 0–255 brightness.
    Values ≤ MIC_THRESHOLD → 0.
    Values ≥ MIC_THRESHOLD+RANGE → 255.
    Linear in between.
    """
    if acc <= MIC_THRESHOLD:
        return 0
    if acc >= MIC_THRESHOLD + RANGE:
        return 255
    # linear interpolation
    return round((acc - MIC_THRESHOLD) * 255 / RANGE)

@pytest.mark.parametrize("acc, expected", [
    (MIC_THRESHOLD - 1,      0),    # below threshold
    (MIC_THRESHOLD,          0),    # at threshold
    (MIC_THRESHOLD + 5000,  128),   # halfway
    (MIC_THRESHOLD + RANGE, 255),   # at max
    (MIC_THRESHOLD + RANGE + 1, 255) # above max
])
def test_map_acc_to_brightness(acc, expected):
    assert map_acc_to_brightness(acc) == expected