// TODO: Debug Row 5 of seed 860446880 ... there are 4 translate text goals? Are the tags set up properly?
//	Or do we have a bug?

// TODO: Inline counters for goals with X subtasks
// TODO: Double click a header for a popout (a la SRL)

function assert(value) {
	if (!value) {
		debugger;
		alert("Assertion failed. Please contact @ClysmiC11 on Twitter with this card's seed so he can debug!");
	}
}

function getRandomSeed(difficulty) {
	// NOTE: We only get a seed from 0 - a hundred million because we are going to append
	//	a difficulty suffix. Which puts us up to a max of a billion, which is fine for an s32.

	let generatedSeed = Math.floor(Math.random() * 100000000);

	if (difficulty === "Easy") {
		generatedSeed += g_suffixEasy;
	}
	else if (difficulty === "Hard") {
		generatedSeed += g_suffixHard;
	}
	else {
		generatedSeed += g_suffixNormal;
	}

	return generatedSeed;
}

function shorthandLookup(strShort) {
	for (let i = 0; i < manifest.shorthands.length; i++) {
		if (manifest.shorthands[i].short === strShort) {
			return manifest.shorthands[i].long;
		}
	}

	return "";
}

function htmlFromStringWithShorthands(str) {
	let iLBrack = -1;
	let iRBrack = -1;
	let result = "";

	for (let i = 0; i < str.length; i++) {
		let c = str[i];

		if (c === "[") {
			iLBrack = i;
		}
		else if (c === "]") {
			iRBrack = i;

			if (iLBrack !== -1) {
				let strShort = str.substring(iLBrack + 1, iRBrack);
				result += "<span class='ttCrumb'>" + strShort + "<span class='ttText'>" + shorthandLookup(strShort) + "</span></span>";
				iLBrack = -1;
				iRBrack = -1;
			}
		}
		else if (iLBrack === -1) {
			result += c;
		}
	}

	return result;
}

function htmlFromGoal(goal) {
	let result = htmlFromStringWithShorthands(goal.text);

	if (goal.alt !== "") {
		result += " <span class='ttCrumb'>[?]<span class='ttText'>" + htmlFromStringWithShorthands(goal.alt) + "</span></span>";
	}

	return result;
}

function initSeed() {
	// Get random seed from URL or generate one

	if (window.location.href.includes("?seed=")) {
		let seedIndex = window.location.href.indexOf("?seed=");
		seedIndex += "?seed=".length;

		let seedString = window.location.href.substring(seedIndex);

		// TODO: Verify this parseInt succeeds?
		// TODO: Handle empty string?

		seedRng(parseInt(seedString));

		if (seedString.charAt(seedString.length - 1) === g_suffixEasy) {
			g_difficulty = "Easy";
		}
		else if ((seedString.charAt(seedString.length - 1) === g_suffixHard)) {
			g_difficulty = "Hard";
		}

		// Make the difficulty text correct

		let spanDifficulty = document.getElementById("spanDifficulty");
		spanDifficulty.innerText = g_difficulty;
	}
	else {
		// NOTE (this performs a redirect)

		window.location.replace(window.location.href + "?seed=" + getRandomSeed("Normal"));
		return;
	}
}

function initLayout() {

	// Set cell size

	{
		let dimension = 0.7 * Math.min(window.innerWidth, window.innerHeight);
		dimension = Math.max(400, dimension);
		dimension = Math.min(800, dimension);

		let cellDimension = dimension / 5.0;

		let cells = document.getElementsByTagName("td");
		for (let i = 0; i < cells.length; i++) {
			cells[i].style.width = cellDimension + "px";
			cells[i].style.height = cellDimension + "px";
		}
	}

	// Resize right panel

	let panelCard = document.getElementById("panelCard");
	let panelRight = document.getElementById("panelRight");

	panelRight.style.height = panelCard.offsetHeight + 0.5 * (window.innerHeight - panelCard.offsetHeight) + "px";
	panelRight.style.width = (window.innerWidth - panelCard.offsetWidth - 50) + "px";
}

function initHandlers() {

	// Initialize new card button

	let btns = document.getElementsByClassName("btnNewCard");
	for (let iBtn = 0; iBtn < btns.length; iBtn++) {
		let btn = btns[iBtn];
		btn.addEventListener("click", function (event) {
			let locationSubstring = window.location.href;
			if (window.location.href.includes("?seed=")) {
				locationSubstring = locationSubstring.substring(0, window.location.href.indexOf("?seed="));
			}

			let difficultySelected = "Normal";
			if (event.target.id === "btnNewEasyCard") {
				difficultySelected = "Easy";
			}
			else if (event.target.id === "btnNewHardCard") {
				difficultySelected = "Hard";
			}
			else {
				assert(event.target.id === "btnNewNormalCard");
			}

			// NOTE (this performs a redirect)

			window.location.replace(locationSubstring + "?seed=" + getRandomSeed(difficultySelected));
			return;
		});
	}

	// Initialize row/column headers

	function cellsFromHeaderId(headerId) {
		let cells = [];

		if (headerId === "tlbr") {
			cells.push(document.getElementById("0_0"));
			cells.push(document.getElementById("1_1"));
			cells.push(document.getElementById("2_2"));
			cells.push(document.getElementById("3_3"));
			cells.push(document.getElementById("4_4"));
		}
		else if (headerId === "bltr") {
			cells.push(document.getElementById("4_0"));
			cells.push(document.getElementById("3_1"));
			cells.push(document.getElementById("2_2"));
			cells.push(document.getElementById("1_3"));
			cells.push(document.getElementById("0_4"));
		}
		else if (headerId.startsWith("col")) {
			let col = headerId.slice(-1);
			col = parseInt(col) - 1;

			for (let i = 0; i < 5; i++) {
				cells.push(document.getElementById(i + "_" + col));
			}
		}
		else if (headerId.startsWith("row")) {
			let row = headerId.slice(-1);
			row = parseInt(row) - 1;

			for (let i = 0; i < 5; i++) {
				cells.push(document.getElementById(row + "_" + i));
			}
		}
		else {
			assert(false);
		}

		return cells;
	}

	let headerSelected = null;
	let headers = document.getElementsByTagName("th");
	for (let i = 0; i < headers.length; i++) {
		let cells = cellsFromHeaderId(headers[i].id);

		headers[i].addEventListener("mouseenter", function () {
			for (let i = 0; i < cells.length; i++) {
				cells[i].classList.add("fakeHover");
			}
		});

		headers[i].addEventListener("mouseout", function () {
			for (let i = 0; i < cells.length; i++) {
				cells[i].classList.remove("fakeHover");
				cells[i].classList.remove("preselected");
			}
		});

		headers[i].addEventListener("mousedown", function () {
			for (let i = 0; i < cells.length; i++) {
				cells[i].classList.add("preselected");
			}
		});

		headers[i].addEventListener("mouseup", function () {
			for (let i = 0; i < cells.length; i++) {
				cells[i].classList.remove("preselected");
			}
		});

		headers[i].addEventListener("click", function () {
			if (headerSelected !== null) {

				assert(headerSelected.classList.contains("selected"));
				headerSelected.classList.remove("selected");

				let cellsPrev = cellsFromHeaderId(headerSelected.id);
				for (let i = 0; i < cellsPrev.length; i++) {
					cellsPrev[i].classList.remove("selected");
				}
			}

			if (headerSelected === this) {
				headerSelected = null;
			}
			else {
				headerSelected = this;

				assert(!headerSelected.classList.contains("selected"));
				headerSelected.classList.add("selected");

				for (let i = 0; i < cells.length; i++) {
					cells[i].classList.add("selected");
				}
			}
		});
	}

	// Init click listener for goals

	let goalCells = [];
	for (let i = 0; i < 5; i++) {
		goalCells.push([]);

		for (let j = 0; j < 5; j++) {
			let cell = document.getElementById(i + "_" + j);
			cell.addEventListener("click", function () {
				if (this.classList.contains("greenCell")) {
					this.classList.add("redCell");
					this.classList.remove("greenCell");
				}
				else if (this.classList.contains("redCell")) {
					this.classList.remove("redCell");
				}
				else {
					this.classList.add("greenCell");
				}
			});
		}
	}

	// Recalculate layout on window resize

	addEventListener("resize", initLayout);
}

function goalsFromHeaderId(headerId) {
	let goals = [];

	if (headerId === "tlbr") {
		goals.push(g_goalGrid[0][0]);
		goals.push(g_goalGrid[1][1]);
		goals.push(g_goalGrid[2][2]);
		goals.push(g_goalGrid[3][3]);
		goals.push(g_goalGrid[4][4]);
	}
	else if (headerId === "bltr") {
		goals.push(g_goalGrid[4][0]);
		goals.push(g_goalGrid[3][1]);
		goals.push(g_goalGrid[2][2]);
		goals.push(g_goalGrid[1][3]);
		goals.push(g_goalGrid[0][4]);
	}
	else if (headerId.startsWith("col")) {
		let col = headerId.slice(-1);
		col = parseInt(col) - 1;

		for (let i = 0; i < 5; i++) {
			goals.push(g_goalGrid[i][col]);
		}
	}
	else if (headerId.startsWith("row")) {
		let row = headerId.slice(-1);
		row = parseInt(row) - 1;

		for (let i = 0; i < 5; i++) {
			goals.push(g_goalGrid[row][i]);
		}
	}
	else {
		assert(false);
	}

	return goals;
}

function computeScoreWithSynergies(headerId) {

	// @Slow
	function getSynergy(goal0, goal1) {
		let synergyMax = Number.NEGATIVE_INFINITY;

		for (let iTag = 0; iTag < goal0.tags.length; iTag++) {
			let tagid = goal0.tags[iTag];
			let tag = manifest.tags[tagid];

			for (let iTagOther = 0; iTagOther < goal1.tags.length; iTagOther++) {
				let tagidOther = goal1.tags[iTagOther];
				let tagOther = manifest.tags[tagidOther];

				for (let iTagSynergy = 0; iTagSynergy < tag.synergies.length; iTagSynergy++) {
					let synergy = tag.synergies[iTagSynergy];

					if (synergy.tagidOther === tagidOther) {
						synergyMax = Math.max(synergyMax, synergy.synergy);
					}
				}
			}
		}

		return (synergyMax === Number.NEGATIVE_INFINITY) ? 0 : synergyMax;
	}

	let result = 0;
	let goals = goalsFromHeaderId(headerId);
	assert(goals.length === 5);

	for (let iGoal = 0; iGoal < 5; iGoal++) {
		let goal = goals[iGoal];
		result += goal.score;

		for (let iGoalOther = iGoal + 1; iGoalOther < 5; iGoalOther++) {
			let goalOther = goals[iGoalOther];
			result -= getSynergy(goal, goalOther);
		}
	}

	return result;
}

// @Slow
function chooseGoals() {

	function nextGoal() {
		if (typeof nextGoal.iGoalNext === 'undefined') {
			nextGoal.iGoalNext = 0;
		}

		while (manifest.goals[nextGoal.iGoalNext].cell) {
			nextGoal.iGoalNext += 1;
			nextGoal.iGoalNext %= manifest.goals.length;
		}

		var result = manifest.goals[nextGoal.iGoalNext];
		nextGoal.iGoalNext += 1;
		nextGoal.iGoalNext %= manifest.goals.length;
		return result;
	}

	function setGoal(row, col, goal) {
		assert(goal.cell === null);

		let oldGoal = g_goalGrid[row][col];
		if (oldGoal !== null) {
			assert(oldGoal.cell);
			oldGoal.cell = null;
		}

		g_goalGrid[row][col] = goal;
		goal.cell = { row: row, col: col };
	}

	// @Slow
	function getNextReplaceInstruction(headerId) {

		function headerIdsFromCell(row, col) {
			let headerIds = [];
			switch (row) {
				case 0:
					headerIds.push("row1");
					break;

				case 1:
					headerIds.push("row2");
					break;

				case 2:
					headerIds.push("row3");
					break;

				case 3:
					headerIds.push("row4");
					break;

				case 4:
					headerIds.push("row5");
					break;

				default:
					assert(false);
			}

			switch (col) {
				case 0:
					headerIds.push("col1");
					break;

				case 1:
					headerIds.push("col2");
					break;

				case 2:
					headerIds.push("col3");
					break;

				case 3:
					headerIds.push("col4");
					break;

				case 4:
					headerIds.push("col5");
					break;

				default:
					assert(false);
			}

			if (row === col) {
				headerIds.push("tlbr");
			}

			if (row + col === 4) {
				headerIds.push("bltr");
			}

			assert(headerIds.length === 2 || headerIds.length === 3 || headerIds.length === 4);
			return headerIds;
		}

		function getTagViolationsAndNearViolations(headerId, goalOmitFromConsideration) {
			let result = {
				tagViolations: [],
				nearTagViolations: []		// Tags that are 1 away from exceeding limit
			}

			let goals = goalsFromHeaderId(headerId);
			let mpTagidCGoal = Array(manifest.tags.length).fill(0);

			for (let iGoal = 0; iGoal < 5; iGoal++) {

				// Useful when getting tag constraints when we are replacing a cell

				let goal = goals[iGoal];
				if (goal === goalOmitFromConsideration)
					continue;


				// Check tag's MaxPerRow

				for (let iTag = 0; iTag < goal.tags.length; iTag++) {

					let tagid = goal.tags[iTag];
					mpTagidCGoal[tagid]++;

					let maxTagPerRow = manifest.tags[tagid].maxPerRow;

					if (mpTagidCGoal[tagid] >= maxTagPerRow) {

						let iNtvExisting = result.nearTagViolations.indexOf(tagid);
						let iTvExisting = result.tagViolations.indexOf(tagid);

						if (mpTagidCGoal[tagid] === maxTagPerRow) {
							assert(iNtvExisting === -1);
							assert(iTvExisting === -1);

							result.nearTagViolations.push(tagid);
						}
						else if (mpTagidCGoal[tagid] === maxTagPerRow + 1) {
							assert(iNtvExisting !== -1);
							assert(iTvExisting === -1);

							result.nearTagViolations.splice(iNtvExisting, 1);
							result.tagViolations.push(tagid);
						}
						else {
							assert(iNtvExisting === -1);
							assert(iTvExisting !== -1);
						}
					}
				}
			}

			return result;
		}

		let goals = goalsFromHeaderId(headerId);
		let result = {
			goalReplace: null,
			tagsDisallowed: [],
			score: 0
		};

		// Compute score

		result.score = computeScoreWithSynergies(headerId);

		// Get list of tag violations (and near tag violations)

		let tv;
		let ntv;
		{
			let tvNtv = getTagViolationsAndNearViolations(headerId);
			tv = tvNtv.tagViolations;
			ntv = tvNtv.nearTagViolations;
		}

		if (tv.length > 0) {

			// Find first goal w/ tag that is in violation. It will be our replace candidate.

		LGoalLoop:
			for (let iGoal = 0; iGoal < 5; iGoal++) {
				let goal = goals[iGoal];
				assert(goal.cell);

				for (let iTag = 0; iTag < goal.tags.length; iTag++) {
					let tagid = goal.tags[iTag];

					for (let iTagViolation = 0; iTagViolation < tv.length; iTagViolation++) {
						let tagidViolation = tv[iTagViolation];

						if (tagid === tagidViolation) {

							// Found a goal that contains a violating tag. Mark it for replacement.

							result.goalReplace = goal;
							break LGoalLoop;
						}
					}
				}
			}

			assert(result.goalReplace);		// Surely, by definition, at least one of our goals must have contained the violating tag!
		}
		else if (result.score < g_headerScoreMin) {
			let goalLowestRawScore = goals[0];

			for (let iGoal = 1; iGoal < 5; iGoal++) {
				let goal = goals[iGoal];
				if (goal.score < goalLowestRawScore.score) {
					goalLowestRawScore = goal;
				}
			}

			result.goalReplace = goalLowestRawScore;
		}
		else if (result.score > g_headerScoreMax) {
			let goalHighestRawScore = goals[0];

			for (let iGoal = 1; iGoal < 5; iGoal++) {
				let goal = goals[iGoal];
				if (goal.score > goalHighestRawScore.score) {
					goalHighestRawScore = goal;
				}
			}

			result.goalReplace = goalHighestRawScore;
		}
		else {
			// All good!

			assert(result.goalReplace === null);
			assert(result.tagsDisallowed.length === 0);
			assert(result.score >= g_headerScoreMin && result.score <= g_headerScoreMax);
			return result;
		}

		// Compute tagsDisallowed

		assert(result.goalReplace);

		let headerIdsIntersect = headerIdsFromCell(result.goalReplace.cell.row, result.goalReplace.cell.col);
		for (let iHeaderIdIntersect = 0; iHeaderIdIntersect < headerIdsIntersect.length; iHeaderIdIntersect++) {
			let headerIdIntersect = headerIdsIntersect[iHeaderIdIntersect];
			let tvNtvHeaderIntersect = getTagViolationsAndNearViolations(headerIdIntersect, result.goalReplace);

			for (let iTvHeaderIntersect = 0; iTvHeaderIntersect < tvNtvHeaderIntersect.tagViolations.length; iTvHeaderIntersect++) {
				result.tagsDisallowed.push(tvNtvHeaderIntersect.tagViolations[iTvHeaderIntersect]);
			}

			for (let iNtvHeaderIntersect = 0; iNtvHeaderIntersect < tvNtvHeaderIntersect.nearTagViolations.length; iNtvHeaderIntersect++) {
				result.tagsDisallowed.push(tvNtvHeaderIntersect.nearTagViolations[iNtvHeaderIntersect]);
			}
		}

		// Uniquify tagsDisallowed
		// @Slow(could use Set)

		result.tagsDisallowed.filter(function (value, index, ary) { return ary.indexOf(value) === index; });
		return result;
	}

	function performReplace(headerId, replaceInstruction) {
		let goals = goalsFromHeaderId(headerId);
		let replaceRow = replaceInstruction.goalReplace.cell.row;
		let replaceCol = replaceInstruction.goalReplace.cell.col;

		LTryFindReplacement:
			for (let cTryFindReplacement = 0; cTryFindReplacement < 100; cTryFindReplacement++) {

				let goalCandidate = nextGoal();

				// NOTE (andrew) Eagerly set the new goal so that computeScoreWithSynergies will consider it. If we
				//  end up not wanting to choose this goal, fear not! The next iteration will use a new one!

				setGoal(replaceRow, replaceCol, goalCandidate);

				// Verify that goal doesn't have disallowed tags!

				for (let iTagCandidate = 0; iTagCandidate < goalCandidate.tags.length; iTagCandidate++) {
					for (let iTagDisallowed = 0; iTagDisallowed < replaceInstruction.tagsDisallowed.length; iTagDisallowed++) {
						if (goalCandidate.tags[iTagCandidate] === replaceInstruction.tagsDisallowed[iTagDisallowed]) {
							continue LTryFindReplacement;
						}
					}
				}

				// Verify that he moves us closer to the ideal score!

				let scoreNew = computeScoreWithSynergies(headerId);
				if (replaceInstruction.score >= g_headerScoreMin && replaceInstruction.score <= g_headerScoreMax) {

					// Row score was in range before doing replacment. Make sure it is still in range.

					if (scoreNew < g_headerScoreMin || scoreNew > g_headerScoreMax) {
						continue;
					}
				}
				else if (replaceInstruction.score < g_headerScoreMin) {

					// We don't need to get within a valid range. We just need to get closer.

					if (scoreNew <= replaceInstruction.score) {
						continue;
					}
				}
				else {
					assert(replaceInstruction.score > g_headerScoreMax);

					// We don't need to get within a valid range. We just need to get closer.

					if (scoreNew >= replaceInstruction.score) {
						continue;
					}
				}

				// Our newly found goal meets all of our criteria! Stop searching!

				break;
			}
	}

	// Init extra fields on goals

	for (let i = 0; i < manifest.goals.length; i++) {
		manifest.goals[i].cell = null;
	}

	// Shuffle goals in manifest

	for (let i = 0; i < manifest.goals.length; i++) {
		let iNew = i + rngInt(manifest.goals.length - i);

		var tmp = manifest.goals[i];
		manifest.goals[i] = manifest.goals[iNew];
		manifest.goals[iNew] = tmp;
	}

	// Pick random goals for each cell

	for (let i = 0; i < 5; i++) {
		for (let j = 0; j < 5; j++) {
			setGoal(i, j, nextGoal());
		}
	}

	// Mutate until we have a reasonable card

	let stopMutating = false;
	let iterations = 0;
	let iterationsMax = 50;

	// @Slow. Probably a better way to do this using some sort of constraint satisfaction technique. I'm just using a bunch of heuristics
	//	to get a half-baked brute force/genetic algorithm style solution. Seems to work good enough. It is quite inefficient, but at 5x5 it's fine.
	while (iterations < iterationsMax && !stopMutating) {
		stopMutating = true;

		for (let i = 0; i < g_headerIds.length; i++) {
			let headerId = g_headerIds[i];
			let replaceInstruction = getNextReplaceInstruction(headerId);

			if (replaceInstruction.goalReplace === null)
				continue;

			stopMutating = false;
			performReplace(headerId, replaceInstruction);
		}

		iterations++;
	}

	if (iterations < iterationsMax) {
		console.log("Board balanced after " + (iterations - 1) + " iterations");
	}
	else {
		assert(iterations == iterationsMax);
		assert(!stopMutating);
		console.log("Board imbalanced (" + iterationsMax + " iterations reached)");
	}
}

function buildBoardHtml() {
	for (let iRow = 0; iRow < 5; iRow++) {
		for (let iCol = 0; iCol < 5; iCol++) {
			let cell = document.getElementById(iRow + "_" + iCol);
			let goal = g_goalGrid[iRow][iCol];

			cell.innerHTML = htmlFromGoal(goal);
		}
	}
}

function superSecretDevDebugTool() {

	let isCodeActive = false;

	function toggleSuperSecretDevDebugTool() {
		isCodeActive = !isCodeActive;

		for (let iRow = 0; iRow < 5; iRow++) {
			for (let iCol = 0; iCol < 5; iCol++) {
				let cell = document.getElementById(iRow + "_" + iCol);
				let goal = g_goalGrid[iRow][iCol];

				if (isCodeActive) {
					let sigma = (goal.score - manifest.goalScoreAvg) / manifest.goalScoreStddev;
					sigma = Math.max(sigma, -3);
					sigma = Math.min(sigma, 3);

					let scoreNormalized = (sigma + 3) / 6;
					let r = scoreNormalized * 255;
					let g = (1 - scoreNormalized) * 255;

					cell = document.getElementById(iRow + "_" + iCol);
					cell.style.backgroundColor = "rgb(" + r + ", " + g + ", 0)";

					assert(cell.innerHTML.indexOf("<br>") === -1);		// Trying to toggle on when already on?

					cell.innerHTML += "<br>{" + goal.score + "}";
				}
				else {
					cell.style.removeProperty("background-color");
					let iBr = cell.innerHTML.indexOf("<br>");
					assert(iBr !== -1);									// Trying to toggle off when already off?
					cell.innerHTML = cell.innerHTML.substring(0, iBr)
				}
			}
		}

		for (let iHeaderId = 0; iHeaderId < g_headerIds.length; iHeaderId++) {

			let headerId = g_headerIds[iHeaderId];
			let goals = goalsFromHeaderId(headerId);
			assert(goals.length === 5);

			let headerDom = document.getElementById(headerId);

			if (isCodeActive) {
				let rawScore = 0;
				for (let iGoal = 0; iGoal < 5; iGoal++) {
					let goal = goals[iGoal];
					rawScore += goal.score;
				}

				let scoreWithSynergy = computeScoreWithSynergies(headerId);

				let scoreWithSynergyNormalized = (scoreWithSynergy - g_headerScoreMin) / (g_headerScoreMax - g_headerScoreMin);
				let r = scoreWithSynergyNormalized * 255;
				let g = (1 - scoreWithSynergyNormalized) * 255;

				headerDom.style.backgroundColor = "rgb(" + r + ", " + g + ", 0)";

				assert(headerDom.innerHTML.indexOf("<br>") === -1);		// Trying to toggle on when already on?

				headerDom.innerHTML += "<br>{score:" + scoreWithSynergy.toFixed(2) + "}<br>{raw:" + rawScore.toFixed(2) + "}";
			}
			else {
				headerDom.style.removeProperty("background-color");
				let iBr = headerDom.innerHTML.indexOf("<br>");
				assert(iBr !== -1);										// Trying to toggle off when already off?
				headerDom.innerHTML = headerDom.innerHTML.substring(0, iBr)
			}
		}

		// We added/removed HTML so we need to recalculate layout!

		initLayout();
	}

	let keyLeft = 37;
	let keyUp = 38;
	let keyRight = 39;
	let keyDown = 40;
	let keyA = 65;
	let keyB = 66;
	let keyEsc = 27;

	let konami = [keyUp, keyUp, keyDown, keyDown, keyLeft, keyRight, keyLeft, keyRight, keyB, keyA];
	let iKonami = 0;

	document.addEventListener("keydown",
		function (event) {
			if (event.keyCode === konami[iKonami]) {
				iKonami++;
			}
			else if (event.keyCode === konami[0]) {
				iKonami = 1;
			}
			else {
				iKonami = 0;

				if (event.keyCode == keyEsc && isCodeActive) {
					toggleSuperSecretDevDebugTool();
				}
			}

			if (iKonami === konami.length) {
				toggleSuperSecretDevDebugTool();
				iKonami = 0;
			}
		}
	);

	let debugStartToggledOn = false;
	if (debugStartToggledOn) {
		toggleSuperSecretDevDebugTool();
	}
}

// Set up global state
// BB (andrew) This is pretty janky, but oh well.

let g_suffixEasy = "0";
let g_suffixNormal = "5";		// NOTE (andrew) Any other suffix also defaults to normal. But 5 is the explicit suffix.
let g_suffixHard = "9";

// NOTE (andrew) Difficulty string is capitalized for... historical reasons

let g_difficulty = "Normal";		// EW please let me use enums JavaScript !!!
let g_headerIds = ["row1", "row2", "row3", "row4", "row5", "col1", "col2", "col3", "col4", "col5", "tlbr", "bltr"];
let g_cPairwiseSynergies = 10;									// 5 choose 2
let g_cPairwiseSynergiesPerGoal = g_cPairwiseSynergies / 5;		// NOTE (andrew) Each goal has 4 other goals it can synergize with. But we don't count synergies both ways, so this should equal 2!
let g_avgScorePostSynergy = manifest.goalScoreAvg - manifest.pairwiseGoalSynergyAvg * g_cPairwiseSynergiesPerGoal;

// Set up 5x5 buffer

let g_goalGrid = [];
for (let i = 0; i < 5; i++) {
	g_goalGrid.push([]);
	for (let j = 0; j < 5; j++) {
		g_goalGrid[i].push(null);
	}
}

// Seed and determine difficulty

initSeed();

let g_headerScoreIdeal = g_avgScorePostSynergy * 5;
if (g_difficulty === "Easy") {
	g_headerScoreIdeal *= 0.7;
}

else if (g_difficulty === "Hard") {
	g_headerScoreIdeal *= 1.3;
}

// NOTE (andrew) sqrt is intended to dampen the volatility a little bit. It is definitely a knob that can be tweaked.

let g_headerScoreMin = g_headerScoreIdeal - Math.sqrt(5 * manifest.goalScoreStddev);
let g_headerScoreMax = g_headerScoreIdeal + Math.sqrt(5 * manifest.goalScoreStddev);

initHandlers();
chooseGoals();
buildBoardHtml();
initLayout();
superSecretDevDebugTool();

document.getElementById("panelMain").style.visibility = "visible";