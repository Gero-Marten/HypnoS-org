<p align="center">
  <img src="http://outskirts.altervista.org/forum/ext/dmzx/imageupload/img-files/2/ca292f8/8585091/34788e79c6bbe7cf7bb578c6fb4d11f8.jpg">
</p>

<h1 align="center">HypnoS</h1>

  ### License

HypnoS is based on the Stockfish engine and is distributed under the GNU General Public License v3.0.
See the [LICENSE](./LICENSE) file for details.


  ### HypnoS Overview


HypnoS is a free and strong UCI chess engine derived from Stockfish 
that analyzes chess positions and computes the optimal moves.

HypnoS does not include a graphical user interface (GUI) that is required 
to display a chessboard and to make it easy to input moves. These GUIs are 
developed independently from HypnoS and are available online.

  ### Acknowledgements

This project is built upon the  [Stockfish](https://github.com/official-stockfish/Stockfish)  and would not have been possible without the exceptional work of the Stockfish developers.  
While HypnoS has diverged from the latest upstream version due to structural differences and the integration of a custom learning system, the core foundation, ideas, and architecture remain deeply rooted in Stockfish.  
I am sincerely grateful to the entire Stockfish team for making such an outstanding engine openly available to the community.

* Andrew Grant for the [OpenBench](https://github.com/AndyGrant/OpenBench) platform.

HypnoS development is currently supported through the OpenBench framework.  
OpenBench, created by Andrew Grant, is an open-source Sequential Probability Ratio Testing (SPRT) framework designed for self-play testing of chess engines.  
It leverages distributed computing, allowing anyone to contribute CPU time to support the development of some of the world’s strongest chess engines.


  #### UCI options
  
  ###  Time Contempt

Type: Integer
Default Value: 20
Range: -100 to 100
Description:
This option sets the engine's "time contempt" factor, which influences how aggressively the engine utilizes its allocated time during the search. The value determines whether the engine should conserve time for future moves or spend more time searching for the best move at the current position.

Purpose:
Helps balance time management strategies based on the game situation.
Encourages aggressive play by spending more time in critical positions or conservative play by saving time for later.
How It Works:
Positive Values (e.g., 20):
The engine is more willing to spend time in earlier moves, searching deeper in critical positions.
Suitable for situations where early-game accuracy is critical.
Negative Values (e.g., -20):
The engine becomes more conservative with time usage, preserving resources for later stages of the game.
Useful in positions where time control or efficiency is essential.
Use Cases:
High Positive Values (e.g., 50 to 100):
Encourages deep analysis and aggressive play early in the game.
Ideal for long time controls or when playing for a win.
High Negative Values (e.g., -50 to -100):
Promotes cautious time management and efficiency.
Suitable for shorter time controls or drawish positions.

  ### CTG/BIN Book File

The file name of the first book file which could be a polyglot (BIN) or Chessbase (CTG) book. To disable this book, use: ```<empty>```
If the book (CTG or BIN) is in a different directory than the engine executable, then configure the full path of the book file, example:
```C:\Path\To\My\Book.ctg``` or ```/home/username/path/to/book/bin```

  ### Book Width

The number of moves to consider from the book for the same position. To play best book move, set this option to 1. If a value ```n``` (greater than 1 is configured, the engine will pick **randomly** one of the top ```n``` moves available in the book for the given position

  ### Book Depth

The maximum number of moves to play from the book
	
  ### Self-Learning

*	### Experience file structure:

1. e4 (from start position)
1. c4 (from start position)
1. Nf3 (from start position)
1 .. c5 (after 1. e4)
1 .. d6 (after 1. e4)

2 positions and a total of 5 moves in those positions

Now imagine HypnoS plays 1. e4 again, it will store this move in the experience file, but it will be duplicate because 1. e4 is already stored. The experience file will now contain the following:
1. e4 (from start position)
1. c4 (from start position)
1. Nf3 (from start position)
1 .. c5 (after 1. e4)
1 .. d6 (after 1. e4)
1. e4 (from start position)

Now we have 2 positions, 6 moves, and 1 duplicate move (so effectively the total unique moves is 5)

Duplicate moves are a problem and should be removed by merging with existing moves. The merge operation will take the move with the highst depth and ignore the other ones. However, when the engine loads the experience file it will only merge duplicate moves in memory without saving the experience file (to make startup and loading experience file faster)

At this point, the experience file is considered fragmented because it contains duplicate moves. The fragmentation percentage is simply: (total duplicate moves) / (total unique moves) * 100
In this example we have a fragmentation level of: 1/6 * 100 = 16.67%

  ### Experience Readonly

  Default: False If activated, the experience file is only read.
  
  ### Experience Book

  HypnoS play using the moves stored in the experience file as if it were a book

  ### Experience Book Width

The number of moves to consider from the book for the same position.

  ### Experience Book Eval Importance

The quality of experience book moves has been revised heavily based on feedback from users. The new logic relies on a new parameter called (Experience Book Eval Importance) which defines how much weight to assign to experience move evaluation vs. count.

The maximum value for this new parameter is: 10, which means the experience move quality will be 100% based on evaluation, and 0% based on count

The minimum value for this new parameter is: 0, which means the experience move quality will be 0% based on evaluation, and 100% based on count

The default value for this new parameter is: 5, which means the experience move quality will be 50% based on evaluation, and 50% based on count	

  ### Experience Book Min Depth

Type: Integer
Default Value: 27
Range: Max to 64
This option sets the minimum depth threshold for moves to be included in the engine's Experience Book. Only moves that have been searched at least to the specified depth will be considered for inclusion.

  ### Experience Book Max Moves

Type: Integer
Default Value: 16
Range: 1 to 100
	This is a setup to limit the number of moves that can be played by the experience book.
	If you configure 16, the engine will only play 16 moves (if available).

  ### Variety

Enables randomization of move selection in balanced positions not covered by the opening book.  
A higher value increases the probability of deviating from the mainline, potentially at the cost of Elo.

This option is mainly intended for testing, analysis, or generating varied self-play games.

Set to `0` for fully deterministic behavior.  
Typical useful range: `10–20` for light variety in early game.

 Note: Variety works in combination with `Variety Max Score` and `Variety Max Moves`, which control the conditions under which randomness can apply.
  ### Variety Max Score

Maximum score threshold (in centipawns) below which randomization of the best move is allowed.  
If the absolute evaluation is below this value, the engine may apply a small, controlled random bonus  
to the best move score in order to increase variability in balanced positions.

- A value of `0` disables the feature (fully deterministic behavior).
- Typical values range from `10` to `30`.
- The hard maximum is `50`, beyond which the randomness could affect clearly winning or losing positions and is not recommended.

This feature is primarily intended for testing purposes, to introduce diversity in games that would otherwise be repetitive.

  ### Variety Max Moves

Maximum game ply (half-moves) under which the variety bonus can be applied.  
Once the game progresses beyond this ply count, the randomization feature is disabled.

- `0` disables the feature entirely.
- Values between `10` and `30` are typical for introducing diversity early in the game.
- The hard cap is `40`, since variety in late-game scenarios is generally undesirable.

This setting prevents randomness from affecting important endgame decisions. 

  ###   NNUE Dynamic Weights

Type: Boolean
Default: true
Description: Enables the dynamic Shashin-style blending of Tal/Capablanca/Petrosian weights using game phase and positional indicators.
Interaction: If NNUE ManualWeights is true, this dynamic system is bypassed and the manual weights are used as-is.

  ###   NNUE ManualWeights Adjustment

Type: Boolean (true or false)
Default: false
Enables or disables manual weight adjustments for the engine's evaluation function. When enabled, users can directly modify NNUE StrategyMaterialWeight and NNUE StrategyPositionalWeight.
Usage:
true: NNUE ManualWeights adjustments are enabled.
false: The engine uses dynamic weight calculations based on the game phase and position.

  ###   NNUE StrategyMaterialWeight

Adjusts the weight given to material balance in the evaluation function. Higher values emphasize material over other positional factors.

Type: Integer (Range: -12 to 12)
Default: 0

  ###   NNUE StrategyPositionalWeight

Adjusts the weight given to positional considerations in the evaluation function. Higher values emphasize positional strategy over material.

Type: Integer (Range: -12 to 12)
Default: 0

  ###   SEE Gating Quiet

Type: Boolean
Default: true
Description: Enables a cheap SEE (Static Exchange Evaluation) gate for quiet moves to cut obviously bad non-captures late in the move list. Helps search hygiene by avoiding low-value branches when the node is already large.

  ###   SEE Gating Quiet MoveCount

Type: Integer
Range: 0…200 Default: 12
Description: Applies SEE gating for quiet moves only after this many legal moves have already been considered at the node.

Lower values = gate earlier (more pruning)

Higher values = gate later (more conservative)

  ###   SEE Threshold Quiet

Type: Integer (centipawns)
Range: -1000…1000 Default: 0
Description: Minimum SEE score a quiet move must pass to be searched (after the move-count gate).

Positive values prune more (require the move to “look good” by SEE)

Negative values prune less (allow more borderline quiets)

  ###   ProbCut Calm Filter

Type: Boolean
Default: true
Description: Skips ProbCut in tactically sharp positions (e.g., heavy king pressure), where shallow cutoffs are less reliable. When enabled, ProbCut is used primarily in calmer positions.

  ###   ProbCut Attackers Threshold

Type: Integer
Range: 0…16 Default: 3
Description: Maximum number of attackers on either king to still allow ProbCut.

If the number of attackers exceeds this threshold, the position is considered “sharp” and ProbCut is suppressed.

Lower values = stricter (skip ProbCut more often)

Higher values = looser (allow ProbCut in edgier spots)

  ###   Exploration Parameters

  ###   Use Exploration Factor

Type: Boolean
Default: false
Description: When enabled, activates an exploration bonus during move selection (separate from Variety). Useful for testing/A-B runs or adding light stochastic exploration in practical play.
Note: See the “Exploration Parameters” section for factor/decay tuning; this toggle simply turns the mechanism on or off.

  ###   Exploration Factor

Type: Floating-point (Range: 0.0 to 3.0)
Default: 0.2
Description: Controls the balance between exploration and exploitation during the search. A higher value increases exploration.

  ###   Exploration Decay Factor

Type: Floating-point (Range: 0.1 to 5.0)
Default: 1.0
Description: Modifies how quickly the exploration factor decays over time or search depth. This parameter works in conjunction with the Exploration Factor.

  ###   Dynamic Exploration

Type: Boolean (true or false)
Default: true
Description: Toggles dynamic adjustments for the exploration factor. When enabled, the engine adapts exploration levels based on time constraints and position complexity.

  ###  Shashin Dynamic Style

Type: Boolean (true or false)
Default: true
Description: Enables or disables the Shashin Dynamic Style feature, which dynamically adjusts the engine's playing style (e.g., attacking, balanced, or defensive) based on the current position.

  ###  Use Shashin Dynamic Style

Type: Boolean
Default: true
Description: When enabled, the engine applies the current Shashin dynamic style signal to the search (e.g., move ordering bias, LMR reductions, and selective pruning thresholds) in addition to evaluation. Turning it off keeps the search neutral, even if “Shashin Dynamic Style” is on for evaluation.
Interaction: Works independently of NNUE ManualWeights. If manual NNUE weights are used, the search can still be influenced by Shashin style when this is true.
Tip: For clean A/B tests of search heuristics, set this to false to remove style-driven bias from the search.

  ###   Shashin Style

Type: String (Options: Capablanca, Tal, Petrosian, Custom Blend)

Default: Capablanca
Sets the engine's playing style:
Capablanca: Balanced.
Tal: Aggressive and tactical.
Petrosian: Defensive and positional.
Custom Blend: A mix of styles using custom-defined weights.

  ###   Blend Weight Tal

Type: Integer (Range: 0 to 100)
Default: 70
Description: Specifies the weight of the Tal style in a custom blend.

  ###   Blend Weight Capablanca

Type: Integer (Range: 0 to 100)
Default: 0
Description: Specifies the weight of the Capablanca style in a custom blend.

  ###   OBlend Weight Petrosian

Type: Integer (Range: 0 to 100)
Default: 30
Description: Specifies the weight of the Petrosian style in a custom blend.
